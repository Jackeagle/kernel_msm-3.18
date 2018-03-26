<?php
	$myfile = fopen("readme.txt", "w") or die("unable to open file!");
	$txt = "K";
	fwrite($myfile, $txt);
	fclose($myfile);
?>