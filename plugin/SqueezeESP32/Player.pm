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
	
	# New SETD command 0xfe for display width & height
	if ($id == 0xfe) { 
		$value = (unpack('Cn', $$data_ref))[1];
		if ($value > 100 && $value < 400) {
			$prefs->client($client)->set('width', $value);
			$client->display->modes($client->display->build_modes);
			$client->display->widthOverride(1, $value);
			$client->update;
		} 
		my $height = (unpack('Cnn', $$data_ref))[2];
		$prefs->client($client)->set('height', $height || 0);
		$log->info("Setting player $value" . "x" . "$height for ", $client->name);
	}
	
	$client->SUPER::playerSettingsFrame($data_ref);
}

sub hasScrolling {
	return 1;
}

sub reconnect {
	my $client = shift;
	$client->pluginData('artwork_md5', '');
	$client->SUPER::reconnect(@_);
}	

1;
