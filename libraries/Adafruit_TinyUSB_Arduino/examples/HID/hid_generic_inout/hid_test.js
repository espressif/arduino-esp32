// IMPORTANT: install the dependency via 'npm i node-hid' in the same location as the script
// If the install fails on windows you may need to run 'npm i -g windows-build-tools' first to be able to compile native code needed for this library

var HID = require('node-hid');
var os = require('os')
// list of supported devices
var boards = require('./boards.js')

var isWin = (os.platform() === 'win32');
var devices = HID.devices();

// this will choose any device found in the boards.js file
var deviceInfo = devices.find(anySupportedBoard);
var reportLen = 64;

var message = "Hello World!"

// Turn our string into an array of integers e.g. 'ascii codes', though charCodeAt spits out UTF-16
// This means if you have characters in your string that are not Latin-1 you will have to add additional logic for character codes above 255
var messageBuffer = Array.from(message, function(c){return c.charCodeAt(0)});

// Windows wants you to prepend a 0 to whatever you send
if(isWin){
	messageBuffer.unshift(0)
}

// Some OSes expect that you always send a buffer that equals your report length
// So lets fill up the rest of the buffer with zeros
var paddingBuf = Array(reportLen-messageBuffer.length);
paddingBuf.fill(0)
messageBuffer = messageBuffer.concat(paddingBuf)

// check if we actually found a device and if so send our messageBuffer to it
if( deviceInfo ) {
	console.log(deviceInfo)
	var device = new HID.HID( deviceInfo.path );

	// register an event listener for data coming from the device
	device.on("data", function(data) {
		// Print what we get from the device
		console.log(data.toString('ascii'));
	});

	// the same for any error that occur
	device.on("error", function(err) {console.log(err)});

	// send our message to the device every 500ms
	setInterval(function () {
		device.write(messageBuffer);
	},500)
}


function anySupportedBoard(d) {
	
	for (var key in boards) {
	    if (boards.hasOwnProperty(key)) {
	        if (isDevice(boards[key],d)) {
	        	console.log("Found " + d.product);
	        	return true;
	        }
	    }
	}
	return false;
}


function isDevice(board,d){
	return d.vendorId==board[0] && d.productId==board[1];
}

