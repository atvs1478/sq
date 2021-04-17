package Plugins::SqueezeESP32::FirmwareHelper;

use strict;

use File::Basename qw(basename);
use File::Spec::Functions qw(catfile);
use JSON::XS::VersionOneAndTwo;

use Slim::Utils::Log;
use Slim::Utils::Prefs;

use constant FIRMWARE_POLL_INTERVAL => 3600 * (5 + rand());
use constant GITHUB_RELEASES_URI => "https://api.github.com/repos/sle118/squeezelite-esp32/releases";
use constant GITHUB_ASSET_URI => GITHUB_RELEASES_URI . "/assets/";
use constant GITHUB_DOWNLOAD_URI => "https://github.com/sle118/squeezelite-esp32/releases/download/";
use constant ESP32_STATUS_URI => "http://%s/status.json";

my $FW_DOWNLOAD_REGEX = qr|plugins/SqueezeESP32/firmware/([-a-z0-9-/.]+\.bin)$|i;
my $FW_FILENAME_REGEX = qr/^squeezelite-esp32-.*\.bin(\.tmp)?$/;
my $FW_TAG_REGEX = qr/\b(ESP32-A1S|SqueezeAmp|I2S-4MFlash)\.(16|32)\.(\d+)\.([-a-zA-Z0-9]+)\b/;

my $prefs = preferences('plugin.squeezeesp32');
my $log = logger('plugin.squeezeesp32');

my $initialized;

sub init {
	my ($client) = @_;

	if (!$initialized) {
		$initialized = 1;
		Slim::Web::Pages->addRawFunction($FW_DOWNLOAD_REGEX, \&handleFirmwareDownload);
	}

	# start checking for firmware updates
	Slim::Utils::Timers::setTimer($client, Time::HiRes::time() + 3.0 + rand(3.0), \&initFirmwareDownload);
}

sub initFirmwareDownload {
	my ($client) = @_;

	Slim::Utils::Timers::killTimers($client, \&initFirmwareDownload);

	Slim::Networking::SimpleAsyncHTTP->new(
		sub {
			my $http = shift;
			my $content = eval { from_json( $http->content ) };

			if ($content && ref $content) {
				my $releaseInfo = _getFirmwareTag($content->{version});

				if ($releaseInfo && ref $releaseInfo) {
					prefetchFirmware($releaseInfo);
				}
			}
		},
		sub {
			my ($http, $error) = @_;
			$log->error("Failed to get releases from Github: $error");
		},
		{
			timeout => 10
		}
	)->get(sprintf(ESP32_STATUS_URI, $client->ip));

	Slim::Utils::Timers::setTimer($client, Time::HiRes::time() + FIRMWARE_POLL_INTERVAL, \&initFirmwareDownload);
}

sub prefetchFirmware {
	my ($releaseInfo) = @_;

	return unless $releaseInfo;

	Slim::Networking::SimpleAsyncHTTP->new(
		sub {
			my $http = shift;
			my $content = eval { from_json( $http->content ) };

			if (!$content || !ref $content) {
				$@ && $log->error("Failed to parse response: $@");
			}

			my $regex = $releaseInfo->{model} . '\.' . $releaseInfo->{res} . '\.\d+\.' . $releaseInfo->{branch};
			my $url;
			foreach (@$content) {
				if ($_->{tag_name} =~ /$regex/ && $_->{assets} && ref $_->{assets}) {
					($url) = grep /\.bin$/, map {
						$_->{browser_download_url}
					} @{$_->{assets}};

					last if $url;
				}
			}

			downloadFirmwareFile(sub {
				main::INFOLOG && $log->is_info && $log->info("Pre-cached firmware file: " . $_[0]);
			}, sub {
				my ($http, $error, $url, $code) = @_;
				$error ||= ($http && $http->error) || 'unknown error';
				$url   ||= ($http && $http->url) || 'no URL';

				$log->error(sprintf("Failed to get firmware image from Github: %s (%s)", $error || $http->error, $url));
			}, $url) if $url && $url =~ /^https?/;
		},
		sub {
			my ($http, $error) = @_;
			$log->error("Failed to get releases from Github: $error");
		},
		{
			timeout => 10,
			cache => 1,
			expires => 3600
		}
	)->get(GITHUB_RELEASES_URI);
}

sub handleFirmwareDownload {
	my ($httpClient, $response) = @_;

	my $request = $response->request;

	my $_errorDownloading = sub {
		_errorDownloading($httpClient, $response, @_);
	};

	my $path;
	if (!defined $request || !(($path) = $request->uri =~ $FW_DOWNLOAD_REGEX)) {
		return $_errorDownloading->(undef, 'Invalid request', $request->uri, 400);
	}

	# this is the magic number used on the client to figure out whether the plugin does support download proxying
	if ($path eq '-check.bin' && $request->method eq 'HEAD') {
		$response->code(204);
		$response->header('Access-Control-Allow-Origin' => '*');

		$httpClient->send_response($response);
		return Slim::Web::HTTP::closeHTTPSocket($httpClient);
	}

	main::INFOLOG && $log->is_info && $log->info("Requesting firmware from: $path");

	downloadFirmwareFile(sub {
		my $firmwareFile = shift;
		$response->code(200);
		Slim::Web::HTTP::sendStreamingFile($httpClient, $response, 'application/octet-stream', $firmwareFile, undef, 1);
	}, $_errorDownloading, GITHUB_DOWNLOAD_URI . $path);
}

sub downloadFirmwareFile {
	my ($cb, $ecb, $url, $name) = @_;

	# keep track of the last firmware we requested, to prefetch it in the future
	my $releaseInfo = _getFirmwareTag($url);

	$name ||= basename($url);

	if ($name !~ $FW_FILENAME_REGEX) {
		return $ecb->(undef, 'Unexpected firmware image name: ' . $name, $url, 400);
	}

	my $updatesDir = Slim::Utils::OSDetect::dirsFor('updates');
	my $firmwareFile = catfile($updatesDir, $name);

	my $fileMatchRegex = join('-', '', $releaseInfo->{branch}, $releaseInfo->{model}, $releaseInfo->{res});
	Slim::Utils::Misc::deleteFiles($updatesDir, $fileMatchRegex, $firmwareFile);

	if (-f $firmwareFile) {
		main::INFOLOG && $log->is_info && $log->info("Found cached firmware file");
		return $cb->($firmwareFile);
	}

	Slim::Networking::SimpleAsyncHTTP->new(
		sub {
			my $http = shift;

			if ($http->code != 200 || !-e "$firmwareFile.tmp") {
				return $ecb->($http, $http->mess);
			}

			rename "$firmwareFile.tmp", $firmwareFile or return $ecb->($http, "Unable to rename temporary $firmwareFile file" );

			return $cb->($firmwareFile);
		},
		$ecb,
		{
			saveAs => "$firmwareFile.tmp",
		}
	)->get($url);

	return;
}

sub _getFirmwareTag {
	my ($info) = @_;

	if (my ($model, $resolution, $version, $branch) = $info =~ $FW_TAG_REGEX) {
		my $releaseInfo = {
			model => $model,
			res => $resolution,
			version => $version,
			branch => $branch
		};

		return $releaseInfo;
	}
}

sub _errorDownloading {
	my ($httpClient, $response, $http, $error, $url, $code) = @_;

	$error ||= ($http && $http->error) || 'unknown error';
	$url   ||= ($http && $http->url) || 'no URL';
	$code  ||= ($http && $http->code) || 500;

	$log->error(sprintf("Failed to get data from Github: %s (%s)", $error || $http->error, $url));

	$response->headers->remove_content_headers;
	$response->code($code);
	$response->content_type('text/plain');
	$response->header('Connection' => 'close');
	$response->content('');

	$httpClient->send_response($response);
	Slim::Web::HTTP::closeHTTPSocket($httpClient);
};


1;