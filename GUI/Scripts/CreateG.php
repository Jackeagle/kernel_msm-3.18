<?php
	$myfile = fopen("readme.txt", "w") or die("unable to open file!");
	$txt = "G";
	fwrite($myfile, $txt);
	fclose($myfile);
?>