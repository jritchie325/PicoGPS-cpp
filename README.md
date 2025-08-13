# PicoGPS-cpp
The project is still under development, and we are currently working towards shifting the program to a library format rather than a lot of code that needs to be copied, which would make the main Pico program a big mess.

Check the active issues page to see what is currently being worked on.

###Current Specs
Runs using UART protocols
- uart1 currently on the pico pins **6**_(gpio4, tx)_ and **7**_(gpio5, rx)_ useing the standard NEO-6M **baud-rate** *(9600)*
>[!Note]
>make sure to connect from tx -> rx and rx -> tx <br/>
><img height="400" alt="Screenshot 2025-08-11 115657" src="https://github.com/user-attachments/assets/eb3664af-ffba-4333-b640-1864694d3719" />



## Running Current Version on PICO-W utilizing VSC
*can be done in other compilers, but this is the way I did it*

To set up deployment, use CMake and the Raspberry Pi Pico extension in VSC. You may need to perform a full installation of CMake(I did not initially, but after some other issues, I installed it, and it seemed to help).
- With the project file open in your workspace, go to CMake and select the Configure or Delete Cache and Reconfigure(this will make the build file)
- Next, click into the Pico plugin and select compile
- After this, if there are no build issues, you should have a .uf2 file in the build folder.
- Plug in the Pico in boot SEL mode and copy the .uf2 file to the RPI-RP2 drive.
>[!Tip]
> alternatively run the following in your terminal:  
>```
>copy C:[PATH]\READ_GPS_PICO_W\build\READ_GPS_PICO_W.uf2 D:\
>```
><sub>*replace [PATH] with the path to your project folder*<br/>
>*default boot drive may differ for your system*</sub>
- After this, open your Serial Monitor of choice to get data out of the pico. Set to cl & rf at 115200 baud



##Acknowledgments
Re-wrote Arduino code from [Roberts Smorgasbord](https://youtube.com/playlist?list=PLbIZFv33DzzltuaZ9WFw8cP4ZtGwzoRew&feature=shared)

