<?php
	$myfile = fopen("readme.txt", "w") or die("unable to open file!");
	$txt = "L";
	fwrite($myfile, $txt);
	fclose($myfile);
?>