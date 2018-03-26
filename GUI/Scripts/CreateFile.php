<?php
	$myfile = fopen("readme.txt", "w") or die("unable to open file!");
	$txt = "Hi Fraser again, you have not selected a button!";
	fwrite($myfile, $txt);
	fclose($myfile);
?>