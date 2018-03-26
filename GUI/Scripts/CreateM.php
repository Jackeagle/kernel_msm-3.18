<?php
	$myfile = fopen("readme.txt", "w") or die("unable to open file!");
	$txt = "M";
	fwrite($myfile, $txt);
	fclose($myfile);
?>