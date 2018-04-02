<?php
	$myfile = fopen("readme.txt", "w") or die("unable to open file!");
	$txt = "I";
	fwrite($myfile, $txt);
	fclose($myfile);
?>