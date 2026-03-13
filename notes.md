# Raspberry Pi Notes
## Ken Kobayashi

---

## Init

Download [Raspberry Pi Imager](https://www.raspberrypi.com/software/) and set up SD card with Raspberry Pi OS.

I created an image with the Raspberry Pi OS Lite, as I won't be using the display out. This is to maximize performance for the audio processing.

When creating the OS, set up SSH public key.

``` powershell
ssh-keygen -t ed25519 -C "pi5"
```

Creates a ssh key pair. Enter the public key's (`.pub` file) content into the Pi image when prompted in the Imager.

Power up Pi with SD card with image, then SSH into it:

```powershell
ssh user_name@device_name.local
```

for me:

```powershell
ssh ken@ken-pi.local
```

in the ssh directory, make the file `config`, and add the entry for the login:

```
Host myPi
    HostName device_name.local
    User user_name
```

This lets you ssh into the pi with the shorthand:

```
ssh myPi
```

Check and install updates:

```bash
sudo apt update && sudo apt full-upgrade -y
```

Install tmux on the pi for better SSH experience:

```bash
sudo apt install tmux
```

Create a config file for tmux:

```bash
nano ~/.tmux.conf
```

Add these lines to the file:

```
set -g mouse on
set -as terminal-features ",xterm-256color:clipboard"
set -s escape-time 50
```

The first line lets you adjust terminal borders with the mouse. The last two fix a handshake issue with tmux on the pi and windows terminal.


## Testing Audio Output

I connected my USB audio interface to the Pi. To ensure this works and to get familiar with the interface, I will copy a sound file over SSH to the Pi and play it.

Front a local terminal, send files as follows:

```bash
scp file.wav user@pi:~/destinationfile.wav
```

In the SSH session, check audio device:

```bash
aplay -l
```

Look for the USB audio device, and note the card number and device number.

Play the file (if audio device card 0 and number 0):

```bash
aplay -D plughw:0,0 ~/test.wav
```

You can adjust the volume for audio devices with controls with `alsamixer`.

```bash
alsamixer
```

## Dev Environment

As I want to work on my windows PC for programming without dealing with cross-compilation, I set up remote SSH in VSCode.

First install the Remote Development extension. Press F1, and choose `Remote-SSH: Connect to Host...`. Select the pi ssh login you saved earlier. 
VSCode now opens in the Pi's filesystem, with the VSCode terminal already SSH'ed in. All changes to the files are done in the Pi, and building and running is done on the Pi.

To finish, install `cmake` and ALSA headers on the Pi to be able to compile code for audio:

```bash
sudo apt install build-essential gdb cmake libasound2-dev
```

This is possible because the Pi 5 I'm using is powerful enough to host a VSCode server and compile on its own. This is not possible when using a weaker board.

To practice, I am also using the Vim extension in VSCode, to see if I can get familiar with it.

## Project setup

Make a `CMakeList.txt` to set up the project.

Create a `main.cpp` file with test code:

```cpp
#include <stdio.h>

int main() {
    printf("Hello, World!\n");
    return 0;
}
```

Make build dir:

```bash
mkdir -p build && cd build
cmake ..
```

Install `ninja` to build the project for fast builds:

```bash
sudo apt update
sudo apt install ninja
```

Run `ninja` in build directory

```bash
ninja
```

Run test executable:

```bash
./synth
```

In the future, the executable itself should be run in a separate terminal running tmux so it stays alive on the Pi.