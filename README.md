# ESP32-Websocket-BitcoinTicker

Basic live ticker that connects to bitstamp.net websocket server and displays current bitcoin price. This project uses the Arduino IDE.

<img src="https://github.com/jfduhart/ESP32-Websocket-BitcoinTicker/blob/master/images/BitcoinTicker.png" width="50%" height="50%" />

Board : TTGO T-Display ESP32


(To set up the board, follow these instructions : [LilyGO TTGO T-Display](https://github.com/Xinyuan-LilyGO/TTGO-T-Display) )

## Libraries used in this project

- ArduinoWebsockets 0.4.17 by Gil Maimon 
- ArduinoJson 6.15.1 by Benoit Blanchon
- TFT-eSPI 2.2.0 by Bodmer (Need extra config, follow instructions above)
- Time 1.5.0 by Michael Margolls
- Button2 1.2.0 by Lennart Hennins
- NTPClient 3.2.0 by Fabrice Weinberg
- Other libraries are built-in

## Secure Websocket configuration

Websocket used in this project: [Bitstamp Websocket](https://www.bitstamp.net/websocket/v2/)

In order to establish a socket connection, you must supply the Root CA certificate of the websocket server. I've supplied the current certificate(as of May 2020), but it will change in the future. In order to download, open a terminal/cmd (must have openssl installed) and send the following command:

`  openssl s_client -showcerts -connect ws.bitstamp.net:443 </dev/null `

This will show multiple certificates (delimited by "----BEGIN CERTIFICATE----" and "----END CERTIFICATE----"), be sure to select the Root CA.

## Buttons

Code includes handlers for both simple click and long click behaviours, but they are not used for anything at the moment.

## Time

This board does not include an RTC (Real time clock) module, so time update is done fetching info from an ntp server (pool.ntp.org). utcOffsetInSeconds variable must be configured to your local timezone in order to get you local time correctly displayed.
