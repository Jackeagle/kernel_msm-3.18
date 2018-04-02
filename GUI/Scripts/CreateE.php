<?php
	$myfile = fopen("readme.txt", "w") or die("unable to open file!");
	$txt = "E";
	fwrite($myfile, $txt);
	fclose($myfile);
?>