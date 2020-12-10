# ESP32 + Autopi + DCC50S Battery status monitoring

This repo contains code for ESP32 + Autopi + Renogy DCC50S powered 
battery monitoring system.

ESP32 reads solar charging status from DCC50S and measures voltages
across a shunt which is then calculated to total battery current. This 
data is send to HTTP server running on Autopi.
