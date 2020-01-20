package Plugins::SqueezeESP32::Player;

use strict;
use base qw(Slim::Player::SqueezePlay);

use Slim::Utils::Log;
use Slim::Utils::Prefs;

my $prefs = preferences('plugin.squeezeesp32');

sub model { 'squeezeesp32' }
sub modelName { 'SqueezeESP32' }
sub hasIR { 0 }

# We need to implement this to allow us to receive SETD commands
# and we need SETD to support custom display widths
sub directBodyFrame { 1 }

# Allow the player to define it's display width (and probably more)
sub playerSettingsFrame {
	my $client   = shift;
	my $data_ref = shift;
	
	my $value;
	my $id = unpack('C', $$data_ref);
        
	# New SETD command 0xfe for display width
	if ($id == 0xfe) { 
		$value = (unpack('CC', $$data_ref))[1];
		if ($value > 10 && $value < 200) {
			$client->display->widthOverride(1, $value);
			$client->update;
		} 
	}
}

sub hasScrolling  {
	return 1;
}

1;
