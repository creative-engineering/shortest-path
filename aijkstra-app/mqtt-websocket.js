
let inData = [], outData = [], endNodes = [], pathNodes = []; 
let results = undefined;                                       	//contains json
let numDevice = 49; 		                                       //device interval 0-49 
let msg, device, state;

var client = mqtt.connect({ port: 1884, host: '192.168.31.253' }); //MQTT IP & port

for (let u = 0; u <= numDevice; u++) { inData[u] = "0"; }      //fill up the inData array with 50 * "0"

client.publish("outData", "Browser app ready!");
client.subscribe('inData');                                   //subscribe to topic "inData"


client.on('message', function (topic, message) {

  msg = message.toString(); //e.g. #41,1

  device = msg.slice(1, 3);
  state = msg.slice(4, 5);

  if (device.indexOf("0") == 0) { device = msg.slice(2, 3); } //cut off 04 --> 4 for numbers 0-9

  inData[device] = state;                                     //on pos correlation with the device numb --> put state

console.log("inData: " + inData);

  for (let y = 0; y <= inData.length; y++) {
    if ( inData[y] == "1" ) {		                          //check if device is on (#number,state etc #41,1)
      if ( y !== endNodes[0] && y !== endNodes[1] ) {         //checks if device has been activated before, if not go...
        endNodes.push(y);                                     //push device number to endNodes
        console.log("device: " + y);
      }
    }
    if ( inData[y] == "0" ) {                                 //check if device has been turned off
      if ( y == endNodes[0] || y == endNodes[1] ) {			//do we know this device from previously? if yes pop device
        endNodes.pop(y);
        console.log(endNodes);
      }
    }
  }

  if (endNodes.length == 2)
   {
    results = dijkstra(endNodes[0], endNodes[1]);           //call function (main.js) with endNodes param. 

    for (let k = 0; k < results.path.length; k++) {         //gader all the path nodes
      pathNodes.push(results.path[k].target);
    }
    pathNodes.pop();                                        //remove last path node - most be one of the endNodes

    for (let i = 0; i <= pathNodes.length; i++) {            //Sort pathNodes array	
      if (pathNodes[i] <= 9) {                              //if pathNodes < 10 --> add "0"
        pathNodes[i] = "0" + pathNodes[i]; 
      }
    }

    console.log(pathNodes);

    for (var u = 0; u < pathNodes.length; u++) {              //Publish pathNodes via MQTT
      client.publish("outData", pathNodes[u].toString());
    }

    pathNodes = [], endNodes = [];							  //reset arrays
    for (let u = 0; u <= numDevice; u++) { inData[u] = "0"; } //reset array with all "0"      
  }
});



