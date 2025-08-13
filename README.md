# PicoGPS-cpp
The project is still under development, and we are currently working towards shifting the program to a library format rather than a lot of code that needs to be copied, which would make the main Pico program a big mess.

Check the active issues page to see what is currently being worked on.

To set up deployment, use CMake and the Raspberry Pi Pico extension in VSC. You may need to perform a full installation of CMake(I did not initially, but after some other issues, I installed it, and it seemed to help).
- With the project file open in your workspace, go to CMake and select the Configure or Delete Cache and Reconfigure(this will make the build file)
- Next, click into the Pico plugin and select compile
- After this, if there are no build issues, you should have a .uf2 file in the build folder.
- Plug in the Pico in boot SEL mode and copy the .uf2 file to the RPI-RP2 drive.
- After this, open the Serial Monitor of choice to get data out of the pico.

