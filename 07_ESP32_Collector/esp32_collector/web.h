#pragma once

// The page the phone uses to run the collector, served on the collector's
// own WiFi network. Every web address leads to it (captive portal), so
// phones usually pop it up by themselves after joining the network.
void webInit();
void webLoop();  // call on every loop()
