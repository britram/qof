#!/usr/bin/perl

$project = $ARGV[0] or die;
$license = $ARGV[1] or die;
if ($ARGV[2])
{
    # If provided, we'll only spit out <p:file> elements for this number
    # of releases.  We'll still show history for the others, but won't
    # provide download links for them.
    $relkeep = $ARGV[2];
}

print <<HEAD;
<?xml version="1.0"?>
<p:project xmlns:p="http://netsa.cert.org/xml/project/1.0"
           xmlns="http://www.w3.org/1999/xhtml"
           xmlns:xi="http://www.w3.org/2001/XInclude">
HEAD
    
$ul = 0;
$li = 0;

local $/="undef";
$content = <STDIN>;

$relcount = 0;

# This regexp is pretty liberal, so as to be able to grok most NEWS formats.
while($content =~ /^Version (\d[^:]*?):?\s+\(?([^\n]+?)\)?\s*$\s*=+\s*((?:.(?!Version))+)/msg)
{
    $ver = $1; $date = $2; $notes = $3;
    $relcount++;
    print <<RELHEAD1;
    <p:release>
        <p:version>$ver</p:version>
        <p:date>$date</p:date>
RELHEAD1
;
    if ($relkeep == undef || $relcount <= $relkeep)
    {
    print <<RELHEAD2;
        <p:file href="../releases/$project-$1.tar.gz" license="$license"/>
RELHEAD2

;
    }

    print <<RELHEAD3;
        <p:notes>
            <ul>
RELHEAD3
;
    # First, see if items are delimited by \n\n
    if ($notes =~m@(.+?)\n\n+?@)
    {
        while ($notes =~m@(.+?)\n\n+?@msg)
        {
            print "\t\t<li>$1</li>\n";
        }
        # The last item will be skipped if there aren't two blank lines
        # at the end, so we look for that and fix it here.
        if ($notes =~ /(.+?)(?:\n(?!\n))$/)
        {
            print "\t\t<li>$1</li>\n";
        }
    }
    # Otherwise, assume items are delimited by \n
    else
    {            
        while ($notes =~m@(.*?)\n+@msg)
        {
            print "\t\t<li>$1</li>\n";
        }            
    }

    print <<RELTAIL;
             </ul>
        </p:notes>
    </p:release>        
RELTAIL
;
    
}
print <<TAIL;
</p:project>
TAIL
