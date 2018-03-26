<?php
	$myfile = fopen("readme.txt", "w") or die("unable to open file!");
	$txt = "H";
	fwrite($myfile, $txt);
	fclose($myfile);
?>