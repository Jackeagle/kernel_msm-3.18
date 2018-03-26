<?php
	$myfile = fopen("readme.txt", "w") or die("unable to open file!");
	$txt = "D";
	fwrite($myfile, $txt);
	fclose($myfile);
?>