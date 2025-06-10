# UFBTM
*Unnamed Full Body Tracking modules*


This is a repository for my development of cheap, open-source, and compact realtime tracking modules for motion capture and VR/AR built with ESP32s and the SlimeVR environment.
The components have been chosen to be exclusively through-hole for ease of assembly (at a slight cost of size)
The main IMU this design is based around is the BMI160, primarily for its low cost and small breakout footprint. This IMU is notorious for drifting, so it is also paired with a GY-271 magnetometer to mitigate some of the drift (depending on use location, the magnetometers may need to be disabled due to poor magnetic field).



This idea is by no means novel; I'm not even the first person to use the components I'm using, but I'm attempting to take a cost approach here. The full tracking set (10 modules + 5 extenders) should cost just north of $100 USD without print time costs or filament prices.

Assembly should be quick and reliable with no need for SMD or rework, which is something other modules of this size can't do.












