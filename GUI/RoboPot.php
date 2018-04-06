<!DOCTYPE html>
<html>
<head>
<link rel="stylesheet" href="https://cdnjs.cloudflare.com/ajax/libs/font-awesome/4.7.0/css/font-awesome.min.css">
<style>
.fa {
  padding: 20px;
  font-size: 30px;
  width: 50px;
  text-align: center;
  text-decoration: none;
  margin: 5px 2px;
}
.fa:hover {
    opacity: 0.7;
}
.fa-facebook {
  background: #3B5998;
  color: white;
  float: left;
}
.fa-youtube-square {
  background: red;
  color: white;
  float: left;
}
.fa-github {
	background: black;
	color: white;
	float: left;
}

h1 {
	color: white;
	text-shadow: 1px 1px 5px black, 0 0 25px green, 0 0 5px lawngreen;
	margin-right: 10%;
	font-size: 3em;
	text-align: center;
}
h3 {
	color: white;
	text-shadow: 1px 1px 5px black, 0 0 25px green, 0 0 5px lawngreen;
	text-align: center;
	font-size: 2em;
}
body {
	background-image: url("https://images.pexels.com/photos/413195/pexels-photo-413195.jpeg?h=350&auto=compress&cs=tinysrgb");
}

.button {
    background: #008000; /* Green */
    border: 2px solid white;
    color: white;
    padding: 48px 48px;
    text-align: center;
    text-decoration: none;
    display: inline-block;
    cursor: pointer;
	float: left;
}

.button:hover { 
	opacity: 0.7;
}


.button:after {
    content: "";
    clear: both;
    display: table;
}
.btn-group button:hover { 
	background-color: #3e8e41;
}
.btn-group button:not(:last-child) {
    border-right: none; 
}
p.intro {
	color: white;
	text-shadow: 1px 1px 5px black, 0 0 25px green, 0 0 5px lawngreen;
	front-size: 2.5em;
	text-align: center;
}

table {
	font-family: arial, sans-serif;
	width: 40%;
	margin-left: 30%;
	border-collapse: collapse;
}
td, th {
	border: 1px solid black;
	text-align: left;
	padding: 8px;
}
tr {
	background-color: #4CAF50;
}
</style>
	<title>RoboPot</title>
</head>

<body style="overflow: auto;" onload="position();lightsensing()">

<a href="https://www.facebook.com/ENG5220RoboPot/" class="fa fa-facebook"></a> 
<a href="https://github.com/FrazLaw/RoboPot" class="fa fa-github"></a>
<a href="https://www.youtube.com/channel/UCD5ITvIJ00v1beTOdpyzPOQ" class="fa fa-youtube-square"></a>	
<h1>RoboPot</h1>

<h3>Group 7 - Fraser Law, Fraser Menzies and Alastair Thurlbeck</h3>

<p class="intro">
This is our solution to moveable plant pots remotely controlled via a web interface!
</p>
<table>
	<tr>
		<th>Light Sensor (Position)</th>
		<th>Light Intensity (Lux)</th>
	</tr>
	<tr>
		<td>Top left</td>
		<td id="demo"></td>
		
	</tr>
	<tr>
		<td>Top Right</td>
		<td>"Value?"</td>
	</tr>
	<tr>
		<td>Bottom left</td>
		<td>"Value?"</td>
	</tr>
	<tr>
		<td>Bottom Right</td>
		<td>"Value?"</td>
	</tr>
</table>

<script>

function changeColour(elem) {	
	document.getElementById(elem.id).style.background ="#ff6600";
		setInterval(function(){  
			document.getElementById(elem.id).style.background ="#008000";
		},2000);
}
</script>

<button class="button" style="margin-left: 39.6%" id="A" onclick="myFunction(this)"></button>
<button class="button" id="B" onclick="myFunction(this);"></button>
<button class="button" id="C" onclick="myFunction(this)"></button>
<button class="button" id="D" onclick="myFunction(this)"></button>


<button class="button" style="margin-left: 39.6%" id="E" onclick="myFunction(this)"></button>
<button class="button" id="F" onclick="myFunction(this)"></button>
<button class="button" id="G" onclick="myFunction(this)"></button>
<button class="button" id="H" onclick="myFunction(this)"></button>

<button class="button" style="margin-left: 39.6%" id="I" onclick="myFunction(this)"></button>
<button class="button" id="J" onclick="myFunction(this)"></button>
<button class="button" id="K" onclick="myFunction(this)"></button>
<button class="button" id="L" onclick="myFunction(this)"></button>


<button class="button" style="margin-left: 39.6%" id="M" onclick="myFunction(this)"></button>
<button class="button" id="N" onclick="myFunction(this)"></button>
<button class="button" id="O" onclick="myFunction(this)"></button>
<button class="button" id="P" onclick="myFunction(this)"></button>

<script type="text/javascript">	 //script attribute 
function lightsensing(){
	setInterval(function(){ //wait function 
		var request = new XMLHttpRequest(); //XMLHttpRequest is in-built function
		request.onreadystatechange = function(){
			if (request.readyState == 4 && request.status == 200){ //server status and checks 
				console.log(request.responseText); //request.responseText is the actual value that is being sent, .log isn't actually displaying the data
				document.getElementById("demo").innerHTML =  request.responseText; //document is assigning the value from above into the table 
			}
		}		
		request.open('POST', 'lightsensorvalue1.php', true); //actually opening and sending the files 
		request.send();
	},1000);	
}	//wait
</script>

<script>	//position script
function position(){
	setInterval(function(){ //wait function 
	document.getElementById("A").style.background ="#008000";
	document.getElementById("B").style.background ="#008000";
	document.getElementById("C").style.background ="#008000";
	document.getElementById("D").style.background ="#008000";
	document.getElementById("E").style.background ="#008000";
	document.getElementById("F").style.background ="#008000";
	document.getElementById("G").style.background ="#008000";
	document.getElementById("H").style.background ="#008000";
	document.getElementById("I").style.background ="#008000";
	document.getElementById("J").style.background ="#008000";
	document.getElementById("K").style.background ="#008000";
	document.getElementById("L").style.background ="#008000";
	document.getElementById("M").style.background ="#008000";
	document.getElementById("N").style.background ="#008000";
	document.getElementById("O").style.background ="#008000";
	document.getElementById("P").style.background ="#008000";
		var request = new XMLHttpRequest(); //XMLHttpRequest is in-built function
		request.onreadystatechange = function(){
			if (request.readyState == 4 && request.status == 200){ //server status and checks 
				//console.log(request.responseText); //request.responseText is the actual value that is being sent, .log isn't actually displaying the data
				if (request.responseText == 'A'){
					document.getElementById("A").style.background ="#ff6600";
				} else if (request.responseText == 'B'){
					document.getElementById("B").style.background ="#ff6600";
				} else if (request.responseText == 'C'){
					document.getElementById("C").style.background ="#ff6600";
				} else if (request.responseText == 'D'){
					document.getElementById("D").style.background ="#ff6600";
				} else if (request.responseText == 'E'){
					document.getElementById("E").style.background ="#ff6600";
				} else if (request.responseText == 'F'){
					document.getElementById("F").style.background ="#ff6600";
				} else if (request.responseText == 'G'){
					document.getElementById("G").style.background ="#ff6600";
				} else if (request.responseText == 'H'){
					document.getElementById("H").style.background ="#ff6600";
				} else if (request.responseText == 'I'){
					document.getElementById("I").style.background ="#ff6600";
				} else if (request.responseText == 'J'){
					document.getElementById("J").style.background ="#ff6600";
				} else if (request.responseText == 'K'){
					document.getElementById("K").style.background ="#ff6600";
				} else if (request.responseText == 'L'){
					document.getElementById("L").style.background ="#ff6600";
				} else if (request.responseText == 'M'){
					document.getElementById("M").style.background ="#ff6600";
				} else if (request.responseText == 'N'){
					document.getElementById("N").style.background ="#ff6600";
				} else if (request.responseText == 'O'){
					document.getElementById("O").style.background ="#ff6600";
				} else if (request.responseText == 'P'){
					document.getElementById("P").style.background ="#ff6600";
				} else { 
					document.getElementById("A").style.background ="#ff6600";
				}
			}
		}		
		request.open('POST', 'positionupdate.php', true); //actually opening and sending the files 
		request.send();	
	},3000);		//wait
}
</script>

<script>
function myFunction(elem){
	var xhttp = new XMLHttpRequest();
	xhttp.onreadystatechange = function(){
		if (this.readyState == 4 && this.status == 200){
			document.getElementById(elem.id).innerHTML = 
			this.responseText;
		}
	};
	
	if (elem.id == 'A'){
		xhttp.open("POST", "CreateA.php", true);
		xhttp.send();
	} else if (elem.id == 'B'){
		xhttp.open("POST", "CreateB.php", true);
		xhttp.send();
	} else if (elem.id == 'C'){
		xhttp.open("POST", "CreateC.php", true);
		xhttp.send();
	} else if (elem.id == 'D'){
		xhttp.open("POST", "CreateD.php", true);
		xhttp.send();
	} else if (elem.id == 'E'){
		xhttp.open("POST", "CreateE.php", true);
		xhttp.send();
	} else if (elem.id == 'F'){
		xhttp.open("POST", "CreateF.php", true);
		xhttp.send();
	} else if (elem.id == 'G'){
		xhttp.open("POST", "CreateG.php", true);
		xhttp.send();
	} else if (elem.id == 'H'){
		xhttp.open("POST", "CreateH.php", true);
		xhttp.send();
	} else if (elem.id == 'I'){
		xhttp.open("POST", "CreateI.php", true);
		xhttp.send();
	} else if (elem.id == 'J'){
		xhttp.open("POST", "CreateJ.php", true);
		xhttp.send();
	} else if (elem.id == 'K'){
		xhttp.open("POST", "CreateK.php", true);
		xhttp.send();
	} else if (elem.id == 'L'){
		xhttp.open("POST", "CreateL.php", true);
		xhttp.send();
	} else if (elem.id == 'M'){
		xhttp.open("POST", "CreateM.php", true);
		xhttp.send();
	} else if (elem.id == 'N'){
		xhttp.open("POST", "CreateN.php", true);
		xhttp.send();
	} else if (elem.id == 'O'){
		xhttp.open("POST", "CreateO.php", true);
		xhttp.send();
	} else if (elem.id == 'P'){
		xhttp.open("POST", "CreateP.php", true);
		xhttp.send();
	}else {
		xhttp.open("POST", "CreateFile.php", true);
		xhttp.send();
	}
}

</script>
</body>
</html>
