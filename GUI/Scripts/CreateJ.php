<?php
	$myfile = fopen("readme.txt", "w") or die("unable to open file!");
	$txt = "J";
	fwrite($myfile, $txt);
	fclose($myfile);
?>