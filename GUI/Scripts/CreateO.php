<?php
	$myfile = fopen("readme.txt", "w") or die("unable to open file!");
	$txt = "O";
	fwrite($myfile, $txt);
	fclose($myfile);
?>