const char* Main_Html = "<html>\
<h1><a href = \"http://192.168.4.1/phone\">CELL_PHONE_MODE</h1>\
<h1><a href = \"http://192.168.4.1/vr_key\">VR_KEYBOARD_MODE</h1>\
<h1><a href = \"http://192.168.4.1/vr_joy\">VR_JOYSTICK_MODE</h1>\
</html>";

const char * Phone_Control_Html ="\
			<style>\
input[type=range][orient=vertical] {\
	-webkit-appearance: none;\
	/*-webkit-appearance: slider-vertical; /*if we remove this slider-vertical then horizondally range bar styles applies correctly*/\
	width: 200px;\
	height: 0px;\
	transform: rotate(-90deg);\
	transform-origin:bottom;\
}\
\
input[type=range]::-webkit-slider-thumb {\
	-webkit-appearance: none;\
	border: none;\
	height: 60px;\
	width: 60px;\
	border-radius: 10%;\
	background: black;\
	margin-top: 0px;\
}\
\
input[type=range]::-webkit-slider-runnable-track {\
	background: white;  \
	border: 2px solid black; \
}\
\
input[type=range][orient=horizontal] {\
	-webkit-appearance: none;\
	/*-webkit-appearance: slider-vertical; /*if we remove this slider-vertical then horizondally range bar styles applies correctly*/\
	width: 200px;\
	height: 0px;\
	transform-origin:bottom;  \
}\
</style>\
\
\
<head>\
<script>\
var url = location.href;\
var ACCVal = 5;\
var STRVal = 5;\
var DISTVal = 0;\
\
function Init(){\
  document.getElementById(\"Acc\").innerHTML = ACCVal;\
  document.getElementById(\"Str\").innerHTML = STRVal;\
}\
function Func_Acc_Bar(){\
  ACCVal = document.getElementById(\"Accelerator\").value;\
  Func_Output(ACCVal, STRVal);\
}\
\
function Func_Str_Bar(){\
  STRVal = document.getElementById(\"Steering\").value;\
  Func_Output(ACCVal, STRVal);\
}\
\
\
function Func_Output(ACC, STR){\
  var xhttp = new XMLHttpRequest();\
  document.getElementById(\"Acc\").innerHTML = ACC;\
  document.getElementById(\"Str\").innerHTML = STR;\
  Func_Color(ACC,STR);\
  var url2 = url + \"?ACC=\" + ACC + \"&STR=\" + STR;\
  console.log(url2);\
  xhttp.open(\"GET\", url2, true);\
  xhttp.send();\
}\
\
function Func_Color(ACC, STR){\
  var ACC_Color1 = 0;\
  var ACC_Color = 0;\
  var STR_Color1 = 0;\
  var STR_Color = 0;\
  \
  if(ACC > 5) { \
	ACC_Color1 = parseInt(255 - 255 / 4 * ( ACC - 5 )); \
	ACC_Color = '#'+ACC_Color1.toString(16)+'0000';\
  }\
  else if (ACC == 5) { ACC_Color = '#FFFFFF'; }\
  else { \
	ACC_Color1 = parseInt(255 - 255 / 4 * ( 5 - ACC )); \
	ACC_Color = '#0000'+ACC_Color1.toString(16);\
  }\
\
  if(STR > 5) { \
	STR_Color1 = parseInt(255 - 255 / 4 * ( STR - 5 )); \
	STR_Color = '#'+STR_Color1.toString(16)+'0000';\
  }\
  else if (STR == 5) { STR_Color = '#FFFFFF'; }\
  else { \
	STR_Color1 = parseInt(255 - 255 / 4 * ( 5 - STR )); \
	STR_Color = '#0000'+STR_Color1.toString(16);\
  }\
  console.log(\"STR : \"+ STR_Color+\" ACC : \" + ACC_Color);\
  document.getElementById(\"ACC_Color\").style.backgroundColor = ACC_Color;\
  document.getElementById(\"STR_Color\").style.backgroundColor = STR_Color;\
}\
\
\
function Func_KeyPress(){\
	var keycode=event.keyCode;\
	console.log(\"Key code : \" + keycode);\
	if(keycode == 37) /*Left*/\
	{\
		if ( STRVal >= 2 )\
		{\
			STRVal = Number(STRVal)-1;\
		}\
	}\
	if(keycode == 38) /*Up*/\
	{\
		if ( ACCVal <= 8 )\
		{\
			ACCVal = Number(ACCVal)+1 ;\
		}\
	}\
	if(keycode == 39) /*Right*/\
	{\
		if ( STRVal <= 8 )\
		{\
			STRVal = Number(STRVal) + 1;\
		}\
	}\
	if(keycode == 40) /*Down*/ \
	{\
		if ( ACCVal >= 2 )\
		{\
			ACCVal = Number(ACCVal)-1 ;\
		}\
	}\
	document.getElementById(\"Str\").innerHTML = STRVal;\
	document.getElementById(\"Steering\").innerHTML = STRVal;\
	document.getElementById(\"Acc\").innerHTML = ACCVal;\
	document.getElementById(\"Accelerator\").innerHTML = ACCVal;\
	STRVal_Pre = STRVal;\
	ACCVal_Pre = ACCVal;\
	console.log(\"ACCVal : \"+ ACCVal +\"STRVal : \"+STRVal);\
	Func_Output(ACCVal, STRVal);\
}\
\
document.addEventListener(\"keydown\", function(){Func_KeyPress();} );\
document.addEventListener(\"DOMContentLoaded\", function(){ Dist_Rep(); });\
</script>\
</head>\
\
<body style = \"background-color:#B2CCFF;\" onload = \"Init()\">\
<div align = center><h5> Test Version 2021.01.09 </h5></div><br>\
<table border =0 width = \"100%\">\
<tr>\
<td width = 100 id = \"ACC_Color\" style = \"background-color:#FFFFFF;\">\
<form>\
<div align = \"center\"><input type=\"range\" name=\"slider\" value=\"5\" min=\"1\" max=\"9\" id=\"Accelerator\" oninput=\"Func_Acc_Bar()\" orient=\"vertical\"></div>\
</form>\
</td>\
<td width = 20>\
<font color = red>¡ã<br>F<br>W<br>¡ã<br>¡ã</font><br><font color = blue> <br>¡å<br>¡å<br>B<br>W<br>¡å</font>\
</td>\
<td>\
<div align = center><img src=\"http://192.168.4.1:81/streaming\" width=320 height=240/></div>\
</td>\
<td width = 100 id = \"STR_Color\" style = \"background-color:#FFFFFF;\">\
<div align = \"center\"><input type=\"range\" name=\"slider\" value=\"5\" min=\"1\" max=\"9\" id=\"Steering\" oninput=\"Func_Str_Bar()\" orient=\"horizontal\"></div>\
</td>\
</tr>\
<tr>\
<td>\
</td>\
<td>\
</td>\
<td>\
</td>\
<td>\
<font color = blue>¢¸LEFT¢¸¢¸¢¸¢¸</font><font color = red> ¢º¢º¢º¢ºRIGHT¢º</font>\
</td>\
</tr>\
</table>\
<div align=center><h5>Current Accelerator : <span id=\"Acc\"></span> / Current Steering : <span id=\"Str\"></span></h5></div>\
</body>";

const char * Phone_VR_Html ="\
			<style>\
input[type=range][orient=vertical] {\
	-webkit-appearance: none;\
	/*-webkit-appearance: slider-vertical; /*if we remove this slider-vertical then horizondally range bar styles applies correctly*/\
	width: 200px;\
	height: 0px;\
	transform: rotate(-90deg);\
	transform-origin:bottom;\
}\
\
input[type=range]::-webkit-slider-thumb {\
	-webkit-appearance: none;\
	border: none;\
	height: 60px;\
	width: 60px;\
	border-radius: 10%;\
	background: black;\
	margin-top: 0px;\
}\
\
input[type=range]::-webkit-slider-runnable-track {\
	background: white;  \
	border: 2px solid black; \
}\
\
input[type=range][orient=horizontal] {\
	-webkit-appearance: none;\
	/*-webkit-appearance: slider-vertical; /*if we remove this slider-vertical then horizondally range bar styles applies correctly*/\
	width: 200px;\
	height: 0px;\
	transform-origin:bottom;  \
}\
</style>\
\
\
<head>\
<script>\
var url = location.href;\
var ACCVal = 5;\
var STRVal = 5;\
var DISTVal = 0;\
\
function Init(){\
  document.getElementById(\"Acc\").innerHTML = ACCVal;\
  document.getElementById(\"Str\").innerHTML = STRVal;\
}\
\
function Func_Output(ACC, STR){\
  var xhttp = new XMLHttpRequest();\
  document.getElementById(\"Acc\").innerHTML = ACC;\
  document.getElementById(\"Str\").innerHTML = STR;\
  var url2 = url + \"?ACC=\" + ACC + \"&STR=\" + STR;\
  console.log(url2);\
  xhttp.open(\"GET\", url2, true);\
  xhttp.send();\
}\
\
function Func_KeyPress(){\
	var keycode=event.keyCode;\
	console.log(\"Key code : \" + keycode);\
	if(keycode == 37) /*Left*/\
	{\
		if ( STRVal >= 2 )\
		{\
			STRVal = Number(STRVal)-1;\
		}\
	}\
	if(keycode == 38) /*Up*/\
	{\
		if ( ACCVal <= 8 )\
		{\
			ACCVal = Number(ACCVal)+1 ;\
		}\
	}\
	if(keycode == 39) /*Right*/\
	{\
		if ( STRVal <= 8 )\
		{\
			STRVal = Number(STRVal) + 1;\
		}\
	}\
	if(keycode == 40) /*Down*/ \
	{\
		if ( ACCVal >= 2 )\
		{\
			ACCVal = Number(ACCVal)-1 ;\
		}\
	}\
	document.getElementById(\"Str\").innerHTML = STRVal;\
	document.getElementById(\"Acc\").innerHTML = ACCVal;\
	STRVal_Pre = STRVal;\
	ACCVal_Pre = ACCVal;\
	console.log(\"ACCVal : \"+ ACCVal +\"STRVal : \"+STRVal);\
	Func_Output(ACCVal, STRVal);\
}\
\
document.addEventListener(\"keydown\", function(){Func_KeyPress();} );\
document.addEventListener(\"DOMContentLoaded\", function(){ Dist_Rep(); });\
</script>\
</head>\
\
<table border =0 width = \"100%\">\
<tr>\
<td>\
<img src=\"http://192.168.4.1:81/streaming\" width=480 height=320/>\
</td>\
<td>\
<img src=\"http://192.168.4.1:81/streaming\" width=480 height=320/>\
</td>\
</tr>\
</table>\
<div align=center><h5>Current Accelerator : <span id=\"Acc\"></span> / Current Steering : <span id=\"Str\"></span></h5></div>\
</body>";

const char* Phone_JoyControl_Html = "\
<html>\
<body style=\"background-color:black\">\
<table border =0 width = \"100%\">\
<tr>\
<td>\
<img src=\"http://192.168.4.1:81/streaming\" width=480 height=400/>\
</td>\
<td>\
<img src=\"http://192.168.4.1:81/streaming\" width=480 height=400/>\
</td>\
</tr>\
</table>\
<div align=center><h5>Current Accelerator : <span id=\"Acc\"></span> / Current Steering : <span id=\"Str\"></span></h5></div>\
</body>\
</html>";
