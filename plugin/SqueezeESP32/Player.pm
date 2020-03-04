package Plugins::SqueezeESP32::Player;

use strict;
use base qw(Slim::Player::SqueezePlay);

use Slim::Utils::Log;
use Slim::Utils::Prefs;

my $prefs = preferences('plugin.squeezeesp32');
my $log   = logger('plugin.squeezeesp32');

sub model { 'squeezeesp32' }
sub modelName { 'SqueezeESP32' }
sub hasIR { 0 }

# Allow the player to define it's display width (and probably more)
sub playerSettingsFrame {
	my $client   = shift;
	my $data_ref = shift;
	
	my $value;
	my $id = unpack('C', $$data_ref);
		        
	# New SETD command 0xfe for display width
	if ($id == 0xfe) { 
		$value = (unpack('Cn', $$data_ref))[1];
		if ($value > 100 && $value < 400) {
			$client->display->modes($client->display->build_modes($value));
			$client->display->widthOverride(1, $value);
			$client->update;
		} 
		$log->info("Setting player width $value for ", $client->name);
	}
	
	$client->SUPER::playerSettingsFrame($data_ref);
}

sub hasScrolling  {
	return 1;
}

1;
