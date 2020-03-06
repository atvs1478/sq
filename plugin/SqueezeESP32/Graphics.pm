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

{
	#__PACKAGE__->mk_accessor('array', 'modes');
	__PACKAGE__->mk_accessor('rw', 'modes');
	__PACKAGE__->mk_accessor('rw', qw(vfdmodel));
}

sub new {
	my $class = shift;
	my $client = shift;
	
	my $display = $class->SUPER::new($client);
	my $cprefs = $prefs->client($client);
	
	$cprefs->init( { 
		width => 128,
		small_VU => 15,
		spectrum =>	{	scale => 25,
						small => { size => 25, band => 5.33 },
						full  => { band => 8 },
				},
		}		
	);				
		
	$display->init_accessor(	
		modes => $display->build_modes,
		vfdmodel => 'graphic-<width>x32',	# doesn't matter much
	);	
	
	return $display;
}

=comment
sub modes {
	return \@modes;
}
=cut

sub nmodes {
	return scalar($#{shift->modes()});
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

sub brightnessMap {
	return (65535, 10, 50, 100, 200);
}

=comment
sub bytesPerColumn {
	return 4;
}
=cut

# I don't think LMS renderer handles properly screens other than 32 pixels. It
# seems that all we get is a 32 pixel-tall data with anything else padded to 0
# i.e. if we try 64 pixels height, bytes 0..3 and 4..7 will contains the same 
# pattern than the 32 pixels version, where one would have expected bytes 4..7
# to be empty
sub displayHeight {
	return 32;
}

sub build_modes {
	my $client = shift->client;
	my $cprefs = $prefs->client($client);
	
	my $width = shift || $cprefs->get('width') || 128;
	my $small_VU = $cprefs->get('small_VU');
	my $spectrum = $cprefs->get('spectrum');
	
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
		bar => 0, secs => 0,  width => $width, _width => int -($small_VU*$width/100),
		# extra parameters (width, height, col (< 0 = from right), row (< 0 = from bottom), left_space)
		params => [$VISUALIZER_VUMETER, int ($small_VU*$width/100), 32, int -($small_VU*$width/100), 0, 2] },
		# mode 8
		{ desc => ['VISUALIZER_SPECTRUM_ANALYZER_SMALL'],
		bar => 0, secs => 0,  width => $width, _width => int -($spectrum->{small}->{size}*$width/100),
		# extra parameters (width, height, col (< 0 = from right), row (< 0 = from bottom), left_space, bars)
		params => [$VISUALIZER_SPECTRUM_ANALYZER, int ($spectrum->{small}->{size}*$width/100), 32, int -($spectrum->{small}->{size}*$width/100), 0, 2, int ($spectrum->{small}->{size}/100*$width/$spectrum->{small}->{band}), $spectrum->{scale}/100] },  
		# mode 9	 
		{ desc => ['VISUALIZER_VUMETER'],
		bar => 0, secs => 0,  width => $width,
		params => [$VISUALIZER_VUMETER] },
		# mode 10
		{ desc => ['VISUALIZER_SPECTRUM_ANALYZER'],
		bar => 0, secs => 0,  width => $width,
		# extra parameters (bars)
		params => [$VISUALIZER_SPECTRUM_ANALYZER, int ($width/$spectrum->{full}->{band}), $spectrum->{scale}/100] },	  
		# mode 11	 
		{ desc => ['VISUALIZER_VUMETER', 'AND', 'ELAPSED'],
		bar => 0, secs => 1,  width => $width,
		params => [$VISUALIZER_VUMETER] },
		# mode 12
		{ desc => ['VISUALIZER_SPECTRUM_ANALYZER', 'AND', 'ELAPSED'],
		bar => 0, secs => 1,  width => $width,
		# extra parameters (bars)
		params => [$VISUALIZER_SPECTRUM_ANALYZER, int ($width/$spectrum->{full}->{band}), $spectrum->{scale}/100] },	  
		# mode 13	 
		{ desc => ['VISUALIZER_VUMETER', 'AND', 'REMAINING'],
		bar => 0, secs => -1,  width => $width,
		params => [$VISUALIZER_VUMETER] },
		# mode 14
		{ desc => ['VISUALIZER_SPECTRUM_ANALYZER', 'AND', 'REMAINING'],
		bar => 0, secs => -1,  width => $width,
		# extra parameters (bars)
		params => [$VISUALIZER_SPECTRUM_ANALYZER, int ($width/$spectrum->{full}->{band}), $spectrum->{scale}/100] },	
	);
	
	return \@modes;
}	

1;