#!/usr/local/bin/perl
#Display additional information of GEOS files.

while ($#ARGV >= 0)
{
    $FILE = shift(@ARGV);
    open FILE or die "Can't open $FILE!\n";
    read FILE,$header,254;
    read FILE,$info,254;
    close FILE;

    ($name, $id) = (unpack ("A3A16A11A*", $header)) [1, 3];
    chop $name while ($name =~ /\240$/);
    $name .= ".cvt";

    ($type, $class, $author, $needsclass, $desc) =
	unpack ("x67Cx7A20A20A20x23a*", $info);

    if ($type == 8)
    {
	$author = "";
	$needsclass = "";
    }

    $class =~ s/\0.*$//;
    $author =~ s/\0.*$//;
    $needsclass =~ s/\0.*$//;
    $class =~ tr/\240/ /;
    $author =~ tr/\240/ /;
    $needsclass =~ tr/\240/ /;
    $desc =~ tr/\240/ /;
    $class =~ tr/ //s;
    $author =~ tr/ //s;
    $needsclass =~ tr/ //s;

    $desc =~ s/\0.*$//;
    $desc =~ s/\r/ \n  /gm;
    $desc =~ tr/\0-\11\14-\37//d;

    if ($id =~ /^(PRG|SEQ) formatted GEOS file/) {
	print $name, "\n";
	if ($needsclass ne "") {
	    print "  $needsclass document.";
	}
	elsif ($class ne "") {
	    print "  ", $class, $author ne "" ? " by $author." : ".";
	}
	print "\n  $desc" unless $desc eq "";
	print "\n\n";
    }
}
