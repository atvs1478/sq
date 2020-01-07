package Plugins::SqueezeESP32::Plugin;

use strict;

use base qw(Slim::Plugin::Base);
use Slim::Utils::Prefs;
use Slim::Utils::Log;

my $prefs = preferences('plugin.squeezeesp32');

$prefs->init();

my $log = Slim::Utils::Log->addLogCategory({
	'category'     => 'plugin.squeezeesp32',
	'defaultLevel' => 'INFO',
	'description'  => Slim::Utils::Strings::string('SqueezeESP32'),
}); 

sub initPlugin {
	my $class = shift;

	$class->SUPER::initPlugin(@_);
	Slim::Networking::Slimproto::addPlayerClass($class, 100, 'squeeze2esp32', { client => 'Slim::Player::SqueezePlay', display => 'Slim::Display::Text' });
	LOG_INFO("Added class 100 for SqueezeESP32");
}

1;
