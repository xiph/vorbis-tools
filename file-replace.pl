#!/usr/bin/perl

# parse the arguments:
# file-replace [options] [--] filename[s]|dir[s]
#              --in-pattern="search for"
#              --out-pattern="replace with"
#              --in-pattern-file="file"
#              --out-pattern-file="file"
#              --no-recurse
#              --no-directories


my $search;
my $replace;
my $argflag;
my @infiles;
my $norecurse;
my $nodirs;
my$pwd=`pwd`;
chomp($pwd);

foreach $arg (@ARGV){
    if($arg eq "--"){
	$argflag=1;
	next;
    }

    if(!$argflag && $arg=~/--([^=]*)=(.*)/){
	my$key=$1;
	my$val=$2;

	if($key eq "in-pattern"){
	    $search=$val;
	    next;
	}
	if($key eq "out-pattern"){
	    $replace=$val;
	    next;
	}
	if($key eq "in-pattern-file"){
	    die "Could not open file $val: $!" unless open(F,"$val");
	    undef $/;
	    $search=<F>;
	    $/="\n";
	    close(F);
	    next;
	}
	if($key eq "out-pattern-file"){
	    die "Could not open file $val: $!" unless open(F,"$val");
	    undef $/;
	    $replace=<F>;
	    $/="\n";
	    close(F);
	    next;
	}

	print "Unknown option --$key\n";
	exit(1);
    }
    

    if(!$argflag && $arg=~/--(.*)/){
	if($key eq "no-recurse"){
	    $norecurse;
	    next;
	}
	if($key eq "no-directories"){
	    $nodirs=1;
	    next;
	}
	print "Unknown option --$key\n";
	exit(1);
    }
    
    push @infiles, ($arg);
}

&recursive_doit($pwd,@infiles);

sub recursive_doit{
    my($pwd,@globlist)=@_;
    my @dirs;
    my @files;

    # seperate files from directories
    foreach $file (@globlist){
	if(-d $file){
	    push @dirs,($file);
	    next;
	}
	if(-f $file){
	    push @files,($file);
	    next;
	}
	print "$pwd/$file is not a plain file or directory.\n";
    }

    # Are we called on a directory? recurse?
    if(!$nodirs){
        # fork into each dir with all but the original path ar
	foreach $dir (@dirs){

	    if(fork){
		wait; # don't hose the box ;-)
	    }else{
		die "Could not chdir to $pwd/$dir: $!\n" unless chdir $dir;
		$pwd.="/$dir";
		# open and read the dir
		die "Could not read directory $pwd: $!\n" unless 
		    opendir (D,".");
		#ignore dotfiles
		@globlist=grep { /^[^\.]/ && !(-l "$_") } readdir(D);
		closedir(D);
		$nodirs=$norecurse;

		recursive_doit($pwd,@globlist);
		exit(0);
	    }
	}
    }

    foreach $file (@files){
	if (open(F,$file)){
	    undef $/;
	    my$body=<F>;
	    $/="\n";
	    close(F);

	    # do the regexp
	    if($body=~s{$search}{$replace}g){

		print "Performed substitution on $pwd/$file\n";

		# replace with modified file
		my$tempfile="file-replace-tmp_$$";  
		die $! unless open(F,">$tempfile");
		syswrite F,$body;
		close(F);
		die "Unable to replace modified file $file: $!\n" unless 
		    rename($tempfile,$file);
		unlink $tempfile;
	    }

	}else{
	    print "Could not open $pwd/$file: $!\n";
	}
    }
    exit(0);
}
