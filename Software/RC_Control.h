const char* page = "\
<style>\
input[type=range][orient=vertical]\
{\
    writing-mode: bt-lr; /* IE */\
    -webkit-appearance: slider-vertical; /* WebKit */\
    width: 8px;\
    height: 100px;\
    padding: 0 5px;\
}\
</style>\
\
<table border = 0 width = 800>\
<tr>\
<td>\
<form>\
<input type=\"range\" name=\"slider\" value=\"5\" min=\"0\" max=\"10\" id=\"Accelerator\" oninput=\"Func_Acc()\" orient=\"vertical\"><br>\
</form>\
</td>\
<td>\
Middle\
</td>\
<td>\
<input type=\"range\" name=\"slider\" value=\"5\" min=\"0\" max=\"10\" id=\"Steering\" oninput=\"Func_Str()\" >\
</td>\
</tr>\
</table>\
\
<h4>Current Accelerator : <span id=\"Acc\"></span> / Current Steering : <span id=\"Str\"></span></h4>\
";


const char* page2 = "\
<style>\
input[type=range][orient=vertical]\
{\
    writing-mode: bt-lr; /* IE */\
    -webkit-appearance: slider-vertical; /* WebKit */\
    width: 8px;\
    height: 100px;\
    padding: 0 5px;\
}\
</style>\
\
<table border = 0 width = \"100%\">\
<tr>\
<td width = 200>\
<form>\
<input type=\"range\" name=\"slider\" value=\"5\" min=\"0\" max=\"10\" id=\"Accelerator\" oninput=\"Func_Acc()\" orient=\"vertical\"><br>\
</form>\
</td>\
<td>\
Middle\
</td>\
<td>\
<input type=\"range\" name=\"slider\" value=\"5\" min=\"0\" max=\"10\" id=\"Steering\" oninput=\"Func_Str()\">\
</td>\
</tr>\
</table>\
\
<h4>Current Accelerator : <span id=\"Acc\"></span> / Current Steering : <span id=\"Str\"></span></h4>\
\
<script>\
var url = location.href;\
var ACCVal = 0;\
var STRVal = 0;\
function Func_Acc(){\
  var xhttp = new XMLHttpRequest();\
  ACCVal = document.getElementById(\"Accelerator\").value;\
  document.getElementById(\"Acc\").innerHTML = ACCVal;\
  var url2 = url + \"?ACC=\" + ACCVal + \"&STR=\" + STRVal;\
  xhttp.open(\"GET\", url2, true);\
  xhttp.send();\
}\
\
function Func_Str(){\
  var STRVal = document.getElementById(\"Steering\").value;\
  document.getElementById(\"Str\").innerHTML = STRVal;\
  var xhttp = new XMLHttpRequest();\
  var url2 = url + \"?ACC=\" + ACCVal + \"&STR=\" + STRVal;\
  xhttp.open(\"GET\", url2, true);\
  xhttp.send();\
}\
</script>";
