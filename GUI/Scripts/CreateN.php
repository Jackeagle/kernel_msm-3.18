<?php
	$myfile = fopen("readme.txt", "w") or die("unable to open file!");
	$txt = "N";
	fwrite($myfile, $txt);
	fclose($myfile);
?>