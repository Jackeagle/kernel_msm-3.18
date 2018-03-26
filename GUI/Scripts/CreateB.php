<?php
	$myfile = fopen("readme.txt", "w") or die("unable to open file!");
	$txt = "B";
	fwrite($myfile, $txt);
	fclose($myfile);
?>