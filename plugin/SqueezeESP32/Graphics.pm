package Plugins::SqueezeESP32::Graphics;

use strict;

use base qw(Slim::Display::Squeezebox2);

my $VISUALIZER_NONE = 0;

my @modes = (
	# mode 0
	{ desc => ['BLANK'],
	  bar => 0, secs => 0,  width => 128, 
	  params => [$VISUALIZER_NONE] },
	# mode 1
	{ desc => ['PROGRESS_BAR'],
	  bar => 1, secs => 0,  width => 128,
	  params => [$VISUALIZER_NONE] },
	# mode 2
	{ desc => ['ELAPSED'],
	  bar => 0, secs => 1,  width => 128,
	  params => [$VISUALIZER_NONE] },
	# mode 3
	{ desc => ['ELAPSED', 'AND', 'PROGRESS_BAR'],
	  bar => 1, secs => 1,  width => 128, 
	  params => [$VISUALIZER_NONE] },
	# mode 4
	{ desc => ['REMAINING'],
	  bar => 0, secs => -1, width => 128,
	  params => [$VISUALIZER_NONE] },
	# mode 5  
    { desc => ['CLOCK'],
	  bar => 0, secs => 0, width => 128, clock => 1,
	  params => [$VISUALIZER_NONE] },
	# mode 6	  
	{ desc => ['SETUP_SHOWBUFFERFULLNESS'],
	  bar => 0, secs => 0,  width => 128, fullness => 1,
	  params => [$VISUALIZER_NONE] },
);

sub modes {
	return \@modes;
}

sub nmodes {
	return $#modes;
}

# I don't think LMS renderer handles properly screens other than 32 pixels. It
# seems that all we get is a 32 pixel-tall data with anything else padded to 0
# i.e. if we try 64 pixels height, bytes 0..3 and 4..7 will contains the same 
# pattern than the 32 pixels version, where one would have expected bytes 4..7
# to be empty

=comment
sub bytesPerColumn {
	return 4;
}
=cut

sub displayHeight {
	return 32;
}

sub displayWidth {
	return shift->widthOverride(@_) || 128;
}

sub vfdmodel {
	return 'graphic-128x32';
}

1;