var locationOptions = { "timeout": 15000, "maximumAge": 60000 };

function sendMessage(msg, city) {
  Pebble.sendAppMessage({"temperature": msg, "city":city});
}

function getLocation() {  
  navigator.geolocation.getCurrentPosition(locationSuccess, locationError, locationOptions); 
}

function locationSuccess(pos) {
    getWeather(pos);
}

function locationError(err) {
    sendMessage("24\u00B0","South Africa");
}

function getWeather(position) {
  var lat = position.coords.latitude;
  var lon = position.coords.longitude;

  var url = "http://api.openweathermap.org/data/2.5/weather?units=metric&cnt=1&lat=" + lat + "&lon=" + lon;
  
  var req = new XMLHttpRequest();
  req.open('GET', url, true);
  req.onload = function(e) {
    if (req.readyState == 4 && req.status == 200) {
      var response = JSON.parse(req.responseText);
      var fahrenheit = response.main.temp;
      var temperature = fahrenheit.toFixed(0)+"\u00B0";
      var city = response.name;
      sendMessage(temperature, city);
    }
    else { //retry
      var _req = new XMLHttpRequest();
      _req.open('GET', url, true);
      _req.onload = function(e) {
        if (_req.readyState == 4 && _req.status == 200) {
          var response = JSON.parse(req.responseText);
          var fahrenheit = response.main.temp;
          var temperature = fahrenheit.toFixed(0)+"\u00B0";
          var city = response.name;
          sendMessage(temperature, city);
        }
      };
    }
  };
  req.send(null);
}

Pebble.addEventListener("ready",
  function(e) {
    getLocation(); 
});

Pebble.addEventListener("appmessage",
  function(e) {
    getLocation();
});

Pebble.addEventListener("webviewclosed",
  function(e) {
    getLocation();
});
