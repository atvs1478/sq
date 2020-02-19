package Plugins::SqueezeESP32::Graphics;

use strict;

use base qw(Slim::Display::Squeezebox2);

use Slim::Utils::Prefs;
use Slim::Utils::Log;

my $prefs = preferences('plugin.squeezeesp32');
my $log   = logger('plugin.squeezeesp32');

my $VISUALIZER_NONE = 0;
my $VISUALIZER_VUMETER = 1;
my $VISUALIZER_SPECTRUM_ANALYZER = 2;
my $VISUALIZER_WAVEFORM = 3;

my $width = $prefs->get('width') || 128;
my $spectrum_scale = $prefs->get('spectrum_scale') || 50;

my @modes = (
	# mode 0
	{ desc => ['BLANK'],
	  bar => 0, secs => 0,  width => $width, 
	  params => [$VISUALIZER_NONE] },
	# mode 1
	{ desc => ['PROGRESS_BAR'],
	  bar => 1, secs => 0,  width => $width,
	  params => [$VISUALIZER_NONE] },
	# mode 2
	{ desc => ['ELAPSED'],
	  bar => 0, secs => 1,  width => $width,
	  params => [$VISUALIZER_NONE] },
	# mode 3
	{ desc => ['ELAPSED', 'AND', 'PROGRESS_BAR'],
	  bar => 1, secs => 1,  width => $width, 
	  params => [$VISUALIZER_NONE] },
	# mode 4
	{ desc => ['REMAINING'],
	  bar => 0, secs => -1, width => $width,
	  params => [$VISUALIZER_NONE] },
	# mode 5  
    { desc => ['CLOCK'],
	  bar => 0, secs => 0, width => $width, clock => 1,
	  params => [$VISUALIZER_NONE] },
	# mode 6	  
	{ desc => ['SETUP_SHOWBUFFERFULLNESS'],
	  bar => 0, secs => 0,  width => $width, fullness => 1,
	  params => [$VISUALIZER_NONE] },
	# mode 7
	{ desc => ['VISUALIZER_VUMETER_SMALL'],
	  bar => 0, secs => 0,  width => $width, _width => -20,
	  # extra parameters (width, height, col (< 0 = from right), row (< 0 = from bottom), bars, left space)
	  params => [$VISUALIZER_VUMETER, 20, 32, -20, 0, 2] },
	# mode 8
	{ desc => ['VISUALIZER_SPECTRUM_ANALYZER_SMALL'],
	  bar => 0, secs => 0,  width => $width, _width => -32,
	  # extra parameters (width, height, col (< 0 = from right), row (< 0 = from bottom), bars, left space)
	  params => [$VISUALIZER_SPECTRUM_ANALYZER, 32, 32, -32, 0, 2, 6, $spectrum_scale] },	  
	# mode 9	 
	{ desc => ['VISUALIZER_VUMETER'],
	  bar => 0, secs => 0,  width => $width,
	  params => [$VISUALIZER_VUMETER] },
	# mode 10
	{ desc => ['VISUALIZER_SPECTRUM_ANALYZER'],
	  bar => 0, secs => 0,  width => $width,
	  # extra parameters (bars)
	  params => [$VISUALIZER_SPECTRUM_ANALYZER, 16, $spectrum_scale] },	  
	# mode 11	 
	{ desc => ['VISUALIZER_VUMETER', 'AND', 'ELAPSED'],
	  bar => 0, secs => 1,  width => $width,
	  params => [$VISUALIZER_VUMETER] },
	# mode 12
	{ desc => ['VISUALIZER_SPECTRUM_ANALYZER', 'AND', 'ELAPSED'],
	  bar => 0, secs => 1,  width => $width,
	  # extra parameters (bars)
	  params => [$VISUALIZER_SPECTRUM_ANALYZER, 16, $spectrum_scale] },	  
	# mode 13	 
	{ desc => ['VISUALIZER_VUMETER', 'AND', 'REMAINING'],
	  bar => 0, secs => -1,  width => $width,
	  params => [$VISUALIZER_VUMETER] },
	# mode 14
	{ desc => ['VISUALIZER_SPECTRUM_ANALYZER', 'AND', 'REMAINING'],
	  bar => 0, secs => -1,  width => $width,
	  # extra parameters (bars)
	  params => [$VISUALIZER_SPECTRUM_ANALYZER, 16, $spectrum_scale] },	  	  
	  
);

sub modes {
	return \@modes;
}

sub nmodes {
	return $#modes;
}

sub displayWidth {
	my $display = shift;
	my $client = $display->client;

	# if we're showing the always-on visualizer & the current buttonmode 
	# hasn't overridden, then use the playing display mode to index
	# into the display width, otherwise, it's fullscreen.
	my $mode = 0;
	
	if ( $display->showVisualizer() && !defined($client->modeParam('visu')) ) {
		my $cprefs = preferences('server')->client($client);
		$mode = $cprefs->get('playingDisplayModes')->[ $cprefs->get('playingDisplayMode') ];
	}
	
	if ($display->widthOverride) {
		return $display->widthOverride + ($display->modes->[$mode || 0]{_width} || 0);
	} else {
		return $display->modes->[$mode || 0]{width};
	}	
}

# I don't think LMS renderer handles properly screens other than 32 pixels. It
# seems that all we get is a 32 pixel-tall data with anything else padded to 0
# i.e. if we try 64 pixels height, bytes 0..3 and 4..7 will contains the same 
# pattern than the 32 pixels version, where one would have expected bytes 4..7
# to be empty

sub brightnessMap {
	return (65535, 10, 50, 100, 200);
}

=comment
sub bytesPerColumn {
	return 4;
}
=cut

sub displayHeight {
	return 32;
}

sub vfdmodel {
	return 'graphic-'.$width.'x32';
}

1;