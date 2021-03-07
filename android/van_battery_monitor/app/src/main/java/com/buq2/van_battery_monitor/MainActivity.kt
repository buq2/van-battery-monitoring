package com.buq2.van_battery_monitor


import android.Manifest
import android.bluetooth.*
import android.bluetooth.le.BluetoothLeScanner
import android.bluetooth.le.ScanCallback
import android.bluetooth.le.ScanResult
import android.bluetooth.le.ScanSettings
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.content.pm.PackageManager
import android.location.LocationManager
import android.os.Build
import android.os.Bundle
import android.provider.Settings
import android.util.Log
import android.view.Menu
import android.view.MenuItem
import android.webkit.WebView
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity
import androidx.core.app.ActivityCompat
import androidx.core.content.ContextCompat
import androidx.core.location.LocationManagerCompat


class MainActivity : AppCompatActivity() {
    private val TAG: String = com.buq2.van_battery_monitor.MainActivity::class.java.getSimpleName()
    private var mBluetoothAdapter: BluetoothAdapter? = null
    private var mBluetoothLeScanner: BluetoothLeScanner? = null
    private val PERMISSION_REQUEST_COARSE_LOCATION = 337642
    private val PERMISSION_REQUEST_FINE_LOCATION = 987642
    private val PERMISSION_REQUEST_BACKGROUND_LOCATION = 349875
    private val BLE_DEVICE_NAME: String = "VAN Charge Monitor"
    private val BLE_DEVICE_CHAR: String = "e5d3a406-a784-4bb1-947b-630a9732098a"
    var mBluetoothDevice: BluetoothDevice? = null
    private var myWebView: WebView? = null
    private val REQUEST_ENABLE_BT = 1
    private val REQUEST_ENABLE_LOCATION = 1

    private val mReceiver: BroadcastReceiver = object : BroadcastReceiver() {
        override fun onReceive(context: Context?, intent: Intent) {
            val action = intent.action

            // It means the user has changed his bluetooth state.
            if (action == BluetoothAdapter.ACTION_STATE_CHANGED) {
                if (mBluetoothAdapter!!.getState() === BluetoothAdapter.STATE_TURNING_OFF) {
                    return
                } else if (mBluetoothAdapter!!.getState() === BluetoothAdapter.STATE_OFF) {
                    runOnUiThread {
                        Toast.makeText(applicationContext, "Bluetooth off", Toast.LENGTH_SHORT).show()
                    }
                    return
                } else if (mBluetoothAdapter!!.getState() === BluetoothAdapter.STATE_TURNING_ON) {
                    return
                } else if (mBluetoothAdapter!!.getState() === BluetoothAdapter.STATE_ON) {
                    runOnUiThread {
                        Toast.makeText(applicationContext, "Bluetooth on", Toast.LENGTH_SHORT).show()
                    }
                    StartScan()
                    return
                }
            } else if (action == LocationManager.PROVIDERS_CHANGED_ACTION) {
                if (isLocationOn()) {
                    runOnUiThread {
                        Toast.makeText(applicationContext, "Location on", Toast.LENGTH_SHORT).show()
                    }
                    StartScan()
                } else {
                    runOnUiThread {
                        Toast.makeText(applicationContext, "Location off", Toast.LENGTH_SHORT).show()
                    }
                }
            }
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)
        initBleAndLocation()
        initWebview()
    }

    fun isLocationOn(): Boolean {
        val locationManager = applicationContext.getSystemService(LOCATION_SERVICE) as LocationManager
        return LocationManagerCompat.isLocationEnabled(locationManager)
    }

    fun checkLocation() {
        if (!isLocationOn()) {
            Toast.makeText(this, "Location not enabled", Toast.LENGTH_SHORT).show()
            val intent = Intent(Settings.ACTION_LOCATION_SOURCE_SETTINGS)
            startActivityForResult(intent, REQUEST_ENABLE_LOCATION)
        }
    }

    fun checkBluetooth() {
        if (!mBluetoothAdapter!!.isEnabled()) {
            val enableBtIntent = Intent(BluetoothAdapter.ACTION_REQUEST_ENABLE)
            startActivityForResult(enableBtIntent, REQUEST_ENABLE_BT)
        }
    }

    fun initBleAndLocation() {
        // Use this check to determine whether BLE is supported on the device.  Then you can
        // selectively disable BLE-related features.
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            requestPermissions(arrayOf(Manifest.permission.ACCESS_COARSE_LOCATION), PERMISSION_REQUEST_COARSE_LOCATION);
        }

        if (ContextCompat.checkSelfPermission(this, Manifest.permission.ACCESS_FINE_LOCATION) != PackageManager.PERMISSION_GRANTED) {
            requestPermissions(arrayOf(Manifest.permission.ACCESS_FINE_LOCATION), PERMISSION_REQUEST_FINE_LOCATION);
        }

        // Use this check to determine whether BLE is supported on the device.  Then you can
        // selectively disable BLE-related features.
        if (!packageManager.hasSystemFeature(PackageManager.FEATURE_BLUETOOTH_LE)) {
            Toast.makeText(this, "BLE not supported", Toast.LENGTH_SHORT).show()
            finish()
        }

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            // Android 10+ requires additional permission for ACCESS_BACKGROUND_LOCATION
            if (ContextCompat.checkSelfPermission(baseContext,Manifest.permission.ACCESS_BACKGROUND_LOCATION) != PackageManager.PERMISSION_GRANTED) {
                requestPermissions(arrayOf(Manifest.permission.ACCESS_BACKGROUND_LOCATION), PERMISSION_REQUEST_BACKGROUND_LOCATION)
            }
        }

        // Initializes a Bluetooth adapter.  For API level 18 and above, get a reference to
        // BluetoothAdapter through BluetoothManager.
        val bluetoothManager =
                getSystemService(BLUETOOTH_SERVICE) as BluetoothManager
        mBluetoothAdapter = bluetoothManager.adapter

        // Checks if Bluetooth is supported on the device.
        if (mBluetoothAdapter == null) {
            Toast.makeText(this, "Bluetooth not supported", Toast.LENGTH_SHORT).show()
            finish()
            return
        }

        val filter = IntentFilter()
        filter.addAction(BluetoothAdapter.ACTION_STATE_CHANGED)
        filter.addAction(LocationManager.PROVIDERS_CHANGED_ACTION)
        this.registerReceiver(mReceiver, filter)

        checkBluetooth()
        checkLocation()
        StartScan()
    }


    fun initWebview() {
        myWebView = findViewById(R.id.webview)
        // WebView.setWebContentsDebuggingEnabled(true); // If debugging is needed from Desktop
        myWebView!!.settings.javaScriptEnabled = true
        myWebView!!.settings.domStorageEnabled = true

        // If following are set, webview uses phones native resolution.
        // This is way too small for example Pixel 4A.
        // On the other hand plotlys responsive layout causes problems
        // with Pixel4A if active.
        // myWebView!!.settings.useWideViewPort = true;
        // myWebView!!.settings.loadWithOverviewMode = true;

        myWebView!!.loadUrl("file:///android_asset/index.html")
    }

    fun displayData(data: String) {
        val call = "displayData('" + data + "');"
        myWebView!!.evaluateJavascript(call, null)
    }

    fun StartScan() {
        mBluetoothLeScanner = mBluetoothAdapter!!.getBluetoothLeScanner()
        if (mBluetoothLeScanner == null) {
            Toast.makeText(this, "Bluetooth is off", Toast.LENGTH_SHORT).show()
            return
        }

        val builder = ScanSettings.Builder()
        builder.setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY)
        builder.setReportDelay(0)

        mBluetoothLeScanner!!.stopScan(bluetoothScanCallback);
        mBluetoothDevice = null
        mBluetoothLeScanner!!.startScan(null, builder.build(), bluetoothScanCallback)
    }

    override fun onCreateOptionsMenu(menu: Menu): Boolean {
        // Inflate the menu; this adds items to the action bar if it is present.
        menuInflater.inflate(R.menu.menu_main, menu)
        return true
    }

    override fun onOptionsItemSelected(item: MenuItem): Boolean {
        // Handle action bar item clicks here. The action bar will
        // automatically handle clicks on the Home/Up button, so long
        // as you specify a parent activity in AndroidManifest.xml.
        return when (item.itemId) {
            R.id.action_settings -> true
            else -> super.onOptionsItemSelected(item)
        }
    }

    override fun onResume() {
        super.onResume()
        checkBluetooth()
        checkLocation()
    }

    fun StopScanning() {
        mBluetoothLeScanner!!.stopScan(bluetoothScanCallback);
    }

    fun ConnectToDevice(device: BluetoothDevice) {
        if (mBluetoothDevice != null) {
            return;
        }
        runOnUiThread {
            Toast.makeText(this, "Connected to: " + device.name, Toast.LENGTH_SHORT).show()
        }
        Log.i(TAG, device.name + " " + device.address)
        mBluetoothDevice = device
        device.connectGatt(this, true, gattCallback)
    }


    private val gattCallback: BluetoothGattCallback = object : BluetoothGattCallback() {
        override fun onServicesDiscovered(gatt: BluetoothGatt?, status: Int) {
            // Try to find the service/characteristic in which we are interested
            val services = gatt?.getServices()
            for (gattService in services!!.iterator()) {
                for (gattCharacteristic in gattService.getCharacteristics()) {

                    // We are only interested in this particular characteristic
                    if (gattCharacteristic.uuid.toString() == BLE_DEVICE_CHAR) {

                        // Enable notification for this characteristic
                        // We are also forcing notifications in ESP32 code
                        gatt.setCharacteristicNotification(gattCharacteristic, true)

                        for (desc in gattCharacteristic.getDescriptors()) {
                            desc.apply { value = BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE }
                            gatt.writeDescriptor(desc)
                        }

                    }
                }
            }
        }

        override fun onConnectionStateChange(gatt: BluetoothGatt?, status: Int,
                                             newState: Int) {
            Log.i(TAG, "Connection status changed")

            if (gatt == null) {
                Log.i(TAG, "gatt == null")
            }

            if (newState == BluetoothProfile.STATE_CONNECTED) {
                Log.i(TAG, "Connected")

                // More data please
                gatt?.requestMtu(256);

                // Try to discover services
                gatt?.discoverServices();

            } else if (newState == BluetoothProfile.STATE_DISCONNECTED) {
                Log.i(TAG, "Disconnected")
            }

        }

        override fun onCharacteristicRead(gatt: BluetoothGatt?, characteristic: BluetoothGattCharacteristic?,
                                          status: Int) {
            Log.i(TAG, "Characteristic read")
        }

        override fun onCharacteristicChanged(
                gatt: BluetoothGatt,
                characteristic: BluetoothGattCharacteristic
        ) {
            val data = characteristic.getStringValue(0)
            Log.i(TAG, data)
            runOnUiThread {
                displayData(data)
            }
        }
    }

    private val bluetoothScanCallback: ScanCallback = object : ScanCallback() {
        override fun onScanResult(callbackType: Int, result: ScanResult) {
            val name = result.device.name
            if (name != null && name == BLE_DEVICE_NAME) {
                StopScanning()
                ConnectToDevice(result.device)
            }
        }

        override fun onScanFailed(errorCode: Int) {
            Log.i(TAG, "Scan failed: " + errorCode.toString())
        }
    }
}