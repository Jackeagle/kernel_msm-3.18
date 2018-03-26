<?php
	$myfile = fopen("readme.txt", "w") or die("unable to open file!");
	$txt = "P";
	fwrite($myfile, $txt);
	fclose($myfile);
?>