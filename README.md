# E-Ink Weather and Calendar Display ESP32-S3

## Overview
This project is an ESP32-based E-Ink display (like a Kobo or a Kindle) that shows weather information, Google Calendar events, and random facts. It fetches data from the internet, updates the display, and can be configured for your WiFi, location, and API keys. The whole implementation is built on top of FreeRTOS, ESP-IDF, and the open-source [EPDiy E-Paper Driver](https://github.com/vroland/epdiy).

This was a passion project with the goal of learning as much as possible about embedded devices and programming, and exploring different capabilities of RTOS and the ESP32. As such, a lot of different strategies are used in the code, from event bit groups, multitasking, semaphores and other means of synchronization, and other ESP32 capabilities, such as the WiFi and HTTPS driver, ADC, MBedTLS, timezone handling utils, etc.

As such, obviously, contributions and sugestions are extremely welcome, as I hope to learn much more about common patterns of embedded development.

- [E-Ink Weather and Calendar Display ESP32-S3](#e-ink-weather-and-calendar-display-esp32-s3)
  - [Overview](#overview)
  - [Hardware Requirements](#hardware-requirements)
  - [Software Requirements](#software-requirements)
  - [Project Structure](#project-structure)
  - [Building the Project and Configuring your ESP32](#building-the-project-and-configuring-your-esp32)
    - [1. Set up ESP-IDF, clone this repository and the EPD driver](#1-set-up-esp-idf-clone-this-repository-and-the-epd-driver)
    - [2. Configure the project](#2-configure-the-project)
    - [3. Configure Google OAuth Service Account Private Key](#3-configure-google-oauth-service-account-private-key)
    - [4. Build and flash](#4-build-and-flash)
  - [Usage](#usage)
  - [Roadmap](#roadmap)
  - [License](#license)


**Features:**
- Weather display using Google Weather API 
- Google Calendar event integration through Google OAuth2.0 and service accounts
- Location fetched from API (Static location configuration to be implemented in the future)
- Deep sleep for power saving
- Battery power measurement


## Hardware Requirements
- [LilyGo EPD47 Board](https://github.com/Xinyuan-LilyGO/LilyGo-EPD47) ESP32-S3 with e-ink display
    - This project was developed using the EPD4.7 T5 V2.4, dated 2024-12-03
- Power supply (USB-C or a 3.7V LiPo battery with a JSP2.0 connector can be used)


## Software Requirements
- [ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/) (v5.5 or newer recommended)
- Currently, a custom fork of an older version of the library is used to address issues with the v5 version of ESP-IDF. This will be ported to the official driver as soon as possible.
    - [Used custom EPDiy fork](https://github.com/ogoidmatos/epdiy-rotation-s3)
    - [Original EPDiy Driver](https://github.com/vroland/epdiy)


## Project Structure

```
├── CMakeLists.txt
├── main
│   ├── main.c
│   ├── ui/
│   └── utils/
└── README.md
```
- `main.c` - Main application code, where tasks are started
- `ui/` - Display and UI logic, contains fonts and icons
- `utils/` - Networking functions, JSON parsers, JWT, timezone handling, and task management

## Building the Project and Configuring your ESP32

### 1. Set up ESP-IDF, clone this repository and the EPD driver
   - Follow the [official ESP-IDF setup guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/) for your OS, or use the VSCode extension to configure it.
   - Clone the repository and inside the `components/` directory, clone the EPD driver (for now, the custom fork is required)

### 2. Configure the project
   - Run:
     ```sh
     idf.py menuconfig
     ```
   - Set your WiFi SSID and password, Google Maps Weather API key and service account email, calendar ID, and the refresh interval. (For now, dynamic location is necessary)
   - Create your Google Maps API [here](https://mapsplatform.google.com/lp/maps-apis/). You only need the Weather API enabled for your key.
   - Google Calendar needs a service account to log through OAuth2.0 to provide the needed data. You can follow [this guide](https://developers.google.com/identity/protocols/oauth2/service-account#creatinganaccount) to create a service account. You will need to create a key to register on your ESP32. Configure your client email on the menuconfig.

### 3. Configure Google OAuth Service Account Private Key
  - After creating your key in the step above, download the JSON file that has your account details and your private key.
  - Write your private key into `main/key.pem`, without `"`, as the example file shows, and without replacing the `\n` by line breaks.
  - The private key will be injected to the NVS storage during the build and flash.

### 4. Build and flash
   - Run:
      ```sh
      idf.py build flash
      ```
   - The app should build successfully and flash your device. You will shortly see your device flashing the screen to reset it and fill it with the icons and data.

## Usage

- Power up your ESP32 e-ink device after flashing.
- The device will connect to WiFi, fetch weather and calendar data, and update the display.
- If no calendar events are found, a random fact will be shown.
- The device will enter deep sleep after updating to save power.
- To refresh before the time interval has passed, power cycle the device.
- In the future, the third button will enable display changes. (To be implemented)

## Roadmap
Some features are already planned for the future. If you want to see your feature implemented, you can suggest it in a issue or open a pull request!

- [ ] **Static Location**
  - Currently, you can already choose a static location and input the latitude and longitude in the menuconfig. However, these aren't yet supported in the code. This is planned for the future.
- [ ] **Additional Displays**
  - Additional display layouts are considered. These will be enabled via menuconfig and one can change between them via a button press.
  - Some ideas for new displays are a picture frame, custom stock tracker (?), weather radar (?). Open to new ideas. None are planned to be implemented for now.
- [ ] **Https Client improvement**
  - Improve overall http client implementation for faster comunication and better security and SSL certificate validation. To be implemented soon.
- [ ] **Debug and Error handling improvement**
  - Provide the ability to choose different debug levels on the different tasks and handlers.
  - Implement functionality to save error stack traces to enable debugging for issues that happen when the device is not connected to serial monitor. 
- [ ] **Private Key for Calendar API security improvements**
  - Further improvement in how the private key is stored and handled. Flash encryption to be explored.
- [ ] **Configure Dev Container**
  - Currently, the dev container configuration is not updated. Not planned for now.

## License
This project is licensed under the MIT License. See [LICENSE](LICENSE) for details.

---

For questions or contributions, please open an issue or pull request on the project repository. Ideas for new features are welcome! And if you want me to improve any part of the documentation, please request it, or better yet, contribute! 