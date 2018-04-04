<?php
	$myfile = fopen("readme.txt", "w") or die("unable to open file!");
	$txt = "F";
	fwrite($myfile, $txt);
	fclose($myfile);
?>