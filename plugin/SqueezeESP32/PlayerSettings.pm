package Plugins::SqueezeESP32::PlayerSettings;

use strict;
use base qw(Slim::Web::Settings);
use List::Util qw(first);

use Slim::Utils::Log;
use Slim::Utils::Prefs;

my $sprefs = preferences('server');
my $prefs = preferences('plugin.squeezeesp32');
my $log   = logger('plugin.squeezeesp32');

sub name {
	return Slim::Web::HTTP::CSRF->protectName('PLUGIN_SQUEEZEESP32_PLAYERSETTINGS');
}

sub needsClient {
	return 1;
}

sub validFor {
	my ($class, $client) = @_;
	return $client->model eq 'squeezeesp32' && $client->displayWidth;
}

sub page {
	return Slim::Web::HTTP::CSRF->protectURI('plugins/SqueezeESP32/settings/player.html');
}

sub prefs {
	my ($class, $client) = @_;
	my @prefs = qw(width small_VU spectrum artwork);
	return ($prefs->client($client), @prefs);
}

sub handler {
	my ($class, $client, $paramRef) = @_;
	
	my ($cprefs, @prefs) = $class->prefs($client);
	
	if ($paramRef->{'saveSettings'}) {
		$cprefs->set('small_VU', $paramRef->{'pref_small_VU'});
		my $spectrum =	{	scale => $paramRef->{'pref_spectrum_scale'},
							small => { 	size => $paramRef->{'pref_spectrum_small_size'}, 
										band => $paramRef->{'pref_spectrum_small_band'} },
							full  => { 	band => $paramRef->{'pref_spectrum_full_band'} },
				};
		$cprefs->set('spectrum', $spectrum);
		my $artwork =	{	enable => $paramRef->{'pref_artwork_enable'},
							x => $paramRef->{'pref_artwork_x'}, 
							y => $paramRef->{'pref_artwork_y'},
				};
		$cprefs->set('artwork', $artwork);				
		$client->display->modes($client->display->build_modes);
		$client->display->update;
		
		# force update or disable artwork
		if ($artwork->{'enable'}) {
			Plugins::SqueezeESP32::Plugin::update_artwork($client, 1);
		} else {
			Plugins::SqueezeESP32::Plugin::disable_artwork($client);
		}	
	}
	
	# as there is nothing captured, we need to re-set these variables
	$paramRef->{'pref_width'} = $cprefs->get('width');
	
	# here I don't know why you need to set again spectrum which is a reference
	# to a hash. Using $paramRef->{prefs} does not work either. It seems that 
	# some are copies of value, some are references, can't figure out. This whole
	# logic of "Settings" is beyond me and I really hate it
	$paramRef->{'pref_spectrum'} = $cprefs->get('spectrum');
	$paramRef->{'pref_artwork'} = $cprefs->get('artwork');
	
	return $class->SUPER::handler($client, $paramRef);
}

1;