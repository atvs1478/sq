package Plugins::SqueezeESP32::Player;

use strict;
use base qw(Slim::Player::SqueezePlay);

use Digest::MD5 qw(md5);
use List::Util qw(min);

use Slim::Utils::Log;
use Slim::Utils::Prefs;

my $prefs = preferences('plugin.squeezeesp32');
my $log   = logger('plugin.squeezeesp32');

sub model { 'squeezeesp32' }
sub modelName { 'SqueezeESP32' }
sub hasIR { 0 }

sub init {
	my $client = shift;
	$client->SUPER::init(@_);
	$client->config_artwork();
}

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

			my $height = (unpack('Cnn', $$data_ref))[2];
			$prefs->client($client)->set('height', $height || 0);

			$client->display->modes($client->display->build_modes);
			$client->display->widthOverride(1, $value);
			$client->update;

			main::INFOLOG && $log->is_info && $log->info("Setting player $value" . "x" . "$height for ", $client->name);
		}
	}

	$client->SUPER::playerSettingsFrame($data_ref);
}

sub hasScrolling {
	return 1;
}

sub update_artwork {
	my $client = shift;
	my $cprefs = $prefs->client($client);

	my $artwork = $cprefs->get('artwork') || return;

	return unless $artwork->{'enable'};

	my $s = min($cprefs->get('height') - $artwork->{'y'}, $cprefs->get('width') - $artwork->{'x'});

	my $params = { force => shift || 0 };
	my $path = 'music/current/cover_' . $s . 'x' . $s . '_o.jpg';
	my $body = Slim::Web::Graphics::artworkRequest($client, $path, $params, \&send_artwork, undef, HTTP::Response->new);

	send_artwork($client, undef, \$body) if $body;
}

sub send_artwork {
	my ($client, $params, $dataref) = @_;

	# I'm not sure why we are called so often, so only send when needed
	my $md5 = md5($$dataref);
	return if $client->pluginData('artwork_md5') eq $md5 && !$params->{'force'};

	$client->pluginData('artwork', $dataref);
	$client->pluginData('artwork_md5', $md5);

	my $artwork = $prefs->client($client)->get('artwork') || {};
	my $length = length $$dataref;
	my $offset = 0;

	$log->info("got resized artwork (length: ", length $$dataref, ")");

	my $header = pack('Nnn', $length, $artwork->{'x'}, $artwork->{'y'});

	while ($length > 0) {
		$length = 1280 if $length > 1280;
		$log->info("sending grfa $length");

		my $data = $header . pack('N', $offset) . substr( $$dataref, 0, $length, '' );

		$client->sendFrame( grfa => \$data );
		$offset += $length;
		$length = length $$dataref;
	}
}

sub clear_artwork {
	my ($client, $request) = @_;

	my $artwork = $prefs->client($client)->get('artwork');

	if ($artwork && $artwork->{'enable'}) {
		main::INFOLOG && $log->is_info && $log->info("artwork stop/clear " . $request->getRequestString());
		$client->pluginData('artwork_md5', '');
	}
}

sub config_artwork {
	my ($client) = @_;

	if ( my $artwork = $prefs->client($client)->get('artwork') ) {
		my $header = pack('Nnn', $artwork->{'enable'}, $artwork->{'x'}, $artwork->{'y'});
		$client->sendFrame( grfa => \$header );
	}
}

sub reconnect {
	my $client = shift;
	$client->pluginData('artwork_md5', '');
	$client->SUPER::reconnect(@_);
}

1;
