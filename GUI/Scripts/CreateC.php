<?php
	$myfile = fopen("readme.txt", "w") or die("unable to open file!");
	$txt = "C";
	fwrite($myfile, $txt);
	fclose($myfile);
?>