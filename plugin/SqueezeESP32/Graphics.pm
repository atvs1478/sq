package Plugins::SqueezeESP32::Graphics;

use strict;

use base qw(Slim::Display::Squeezebox2);

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