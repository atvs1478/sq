package Plugins::SqueezeESP32::Plugin;

use strict;

use base qw(Slim::Plugin::Base);
use Slim::Utils::Prefs;
use Slim::Utils::Log;

my $prefs = preferences('plugin.squeezeesp32');

$prefs->init({ 
	width => 128, 
	spectrum_scale => 50,
});

my $log = Slim::Utils::Log->addLogCategory({
	'category'     => 'plugin.squeezeesp32',
	'defaultLevel' => 'INFO',
	'description'  => Slim::Utils::Strings::string('SqueezeESP32'),
}); 

sub initPlugin {
	my $class = shift;
	
	if ( main::WEBUI ) {
		require Plugins::SqueezeESP32::Settings;
		Plugins::SqueezeESP32::Settings->new;
	}

	$class->SUPER::initPlugin(@_);
	Slim::Networking::Slimproto::addPlayerClass($class, 100, 'squeezeesp32', { client => 'Plugins::SqueezeESP32::Player', display => 'Plugins::SqueezeESP32::Graphics' });
	$log->info("Added class 100 for SqueezeESP32");
}

1;
