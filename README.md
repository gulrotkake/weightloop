# Weight Loop

Wii Balance Board measurement loop

https://tightloop.io/balance-board/index.html

## Bluetooth Setup:

````
$ bluetoothctl

power on
agent on
# press red sync button
scan on

pair <MAC of the found wiimote, use TAB for autocompletion>
connect <MAC of the wiimote>
trust <MAC of the wiimote>
disconnect <MAC of the wiimote>
scan off
exit
```

