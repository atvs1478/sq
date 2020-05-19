package Plugins::SqueezeESP32::Plugin;

use strict;

use base qw(Slim::Plugin::Base);

use Slim::Utils::Prefs;
use Slim::Utils::Log;
use Slim::Web::ImageProxy;

my $prefs = preferences('plugin.squeezeesp32');

my $log = Slim::Utils::Log->addLogCategory({
	'category'     => 'plugin.squeezeesp32',
	'defaultLevel' => 'INFO',
	'description'  => 'PLUGIN_SQUEEZEESP32',
});

# migrate 'eq' pref, as that's a reserved word and could cause problems in the future
$prefs->migrateClient(1, sub {
	my ($cprefs, $client) = @_;
	$cprefs->set('equalizer', $cprefs->get('eq'));
	$cprefs->remove('eq');
	1;
});

$prefs->setChange(sub {
	send_equalizer($_[2]);
}, 'equalizer');

sub initPlugin {
	my $class = shift;

	if ( main::WEBUI ) {
		require Plugins::SqueezeESP32::PlayerSettings;
		Plugins::SqueezeESP32::PlayerSettings->new;

		# require Plugins::SqueezeESP32::Settings;
		# Plugins::SqueezeESP32::Settings->new;
	}

	$class->SUPER::initPlugin(@_);
	Slim::Networking::Slimproto::addPlayerClass($class, 100, 'squeezeesp32', { client => 'Plugins::SqueezeESP32::Player', display => 'Plugins::SqueezeESP32::Graphics' });
	main::INFOLOG && $log->is_info && $log->info("Added class 100 for SqueezeESP32");

	# register a command to set the EQ - without saving the values! Send params as single comma separated list of values
	Slim::Control::Request::addDispatch(['squeezeesp32', 'seteq', '_eq'], [1, 0, 0, \&setEQ]);

	# Note for some forgetful know-it-all: we need to wrap the callback to make it unique. Otherwise subscriptions would overwrite each other.
	Slim::Control::Request::subscribe( sub { onNotification(@_) }, [ ['newmetadata'] ] );
	Slim::Control::Request::subscribe( sub { onNotification(@_) }, [ ['playlist'], ['open', 'newsong'] ]);
	Slim::Control::Request::subscribe( \&onStopClear, [ ['playlist'], ['stop', 'clear'] ]);

	# the custom player class is only initialized if it has a display - thus we need to listen to connect events in order to initializes other player prefs
	Slim::Control::Request::subscribe( \&onPlayer,[ ['client'], [ 'new', 'reconnect' ] ] );
}

sub onStopClear {
	my $request = shift;
	my $client  = $request->client || return;

	if ($client->isa('Plugins::SqueezeESP32::Player')) {
		$client->clear_artwork($request);
	}
}

sub onPlayer {
	my $request = shift;
	my $client  = $request->client || return;

	if ($client->model eq 'squeezeesp32') {
		main::INFOLOG && $log->is_info && $log->info("SqueezeESP player connected: " . $client->id);

		$prefs->client($client)->init( {
			equalizer => [(0) x 10],
		} );
		send_equalizer($client);
	}
}

sub onNotification {
	my $request = shift;
	my $client  = $request->client || return;

	if ($client->isa('Plugins::SqueezeESP32::Player')) {
		$client->update_artwork();
	}
}

sub setEQ {
	my $request = shift;

	# check this is the correct command.
	if ($request->isNotCommand([['squeezeesp32'],['seteq']])) {
		$request->setStatusBadDispatch();
		return;
	}

	# get our parameters
	my $client   = $request->client();
	my @eqParams = split(/,/, $request->getParam('_eq') || '');

	for (my $x = 0; $x < 10; $x++) {
		$eqParams[$x] ||= 0;
	}

	send_equalizer($client, \@eqParams);
}

sub send_equalizer {
	my ($client, $equalizer) = @_;

	if ($client->model eq 'squeezeesp32') {
		$equalizer ||= $prefs->client($client)->get('equalizer') || [(0) x 10];
		my $size = @$equalizer;
		my $data = pack("c[$size]", @{$equalizer});
		$client->sendFrame( eqlz => \$data );
	}
}

1;
