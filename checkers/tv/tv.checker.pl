#!/usr/bin/perl

use Mojo::Base -strict;
use Mojo::File 'tempfile';
use Mojo::Log;
use Mojo::UserAgent;

use Time::HiRes 'sleep';

no warnings 'experimental::smartmatch';

my ($SERVICE_OK,   $SERVICE_CORRUPT, $SERVICE_MUMBLE,
    $SERVICE_FAIL, $INTERNAL_ERROR
) = ( 101, 102, 103, 104, 110 );
my %MODES = ( check => \&check, get => \&get, put => \&put );
my ( $mode, $ip ) = splice @ARGV, 0, 2;
my @chars = ( 'A' .. 'Z', 'a' .. 'z', '_', '0' .. '9' );

my $ua  = Mojo::UserAgent->new( max_redirects => 3 );
my $url = Mojo::URL->new("http://$ip:8080/");
my $log = Mojo::Log->new;

warn 'Invalid input. Empty mode or ip address.' and exit $INTERNAL_ERROR
    unless defined $mode and defined $ip;
warn 'Invalid mode.' and exit $INTERNAL_ERROR unless $mode ~~ %MODES;
exit $MODES{$mode}->(@ARGV);

sub _corrupt { say $_[0] and exit $SERVICE_CORRUPT }
sub _mumble  { say $_[0] and exit $SERVICE_MUMBLE }
sub _fail    { say $_[0] and exit $SERVICE_FAIL }

sub _check_http_response {
    my $tx = shift;

    if ( my $err = $tx->error ) {
        if ( $err->{code} ) {
            _mumble "$err->{code} response: $err->{message}";
        } else {
            _fail "Connection error: $err->{message}";
        }
    } else {
        return $tx->success;
    }
}

sub _image {
    my $path = tempfile( 'tempXXXX', SUFFIX => '.png' );
    `convert -size 512x320 xc: -channel G +noise Random -virtual-pixel tile -blur 0x5 -auto-level -separate +channel $path`;
    return ( $path->basename, $path->slurp );
}

sub check {
    $log->info('Getting index page ...');
    $_ = _check_http_response( $ua->get($url) );
    $log->info('Got index page');
    _mumble "Invalid index page" unless $_->text =~ /Hi, I'm TV!/;

    $log->info('Creating user ...');
    $url->path->merge('/create');
    $_ = _check_http_response( $ua->get($url) );
    my ($cookie) = grep { $_->name eq 'user' } @{ $ua->cookie_jar->all };
    my $user = $cookie->value;
    $log->info("Created user: '$user'");
    _mumble "Invalid main page" unless $_->text =~ /Upload new photo/;
    _mumble "Invalid main page" unless $_->text =~ /Latest photos/;

    $log->info('Generating image ...');
    my ( $name, $img ) = _image;
    my $data;
    $data .= $chars[ rand @chars ] for 1 .. 12;

    $url->path->merge('/upload');
    $log->info('Uploading image ...');
    my $res = $ua->post(
        $url => form => {
            info  => $data,
            image => {
                content        => $img,
                filename       => $name,
                'Content-Type' => 'image/png'
            }
        }
    );
    $_ = _check_http_response($res);
    $log->info('Upload image');

    $url->path->merge('/');
    my $ok;
    for ( 1 .. 10 ) {
        $log->info("Getting index page #$_...");
        $_ = _check_http_response( $ua->get($url) );
        $log->info('Got index page');
        if ( $_->text =~ /img/ ) {
            $ok = 1;
            last;
        }
        sleep 0.5;
    }
    _mumble "Invalid image uploading" unless $ok;

    return $SERVICE_OK;
}

sub get {
    my ( $id, $flag ) = @_;

    my ( $user, $sign, $img_id ) = split ':', $id;
    my $tx = $ua->build_tx( GET => $url );
    $tx->req->cookies(
        { name => 'user', value => $user },
        { name => 'sign', value => $sign }
    );
    $log->info('Getting images ...');
    $_ = _check_http_response( $ua->start($tx) );
    $log->info('Got images');
    my $link;
    for my $el ( @{ $_->dom->find('img') } ) {
        $link = $el->attr('src') if $el->attr('src') =~ /$img_id/;
    }
    _corrupt "Invalid image uploading" unless $link;
    $log->info('Found image with flag');

    $url->path->merge($link);
    $log->info('Getting image with flag ...');
    $_ = _check_http_response( $ua->get($url) );
    $log->info('Got image');
    my $path = tempfile( 'tempXXXX', SUFFIX => '.png' );
    $path->spurt($_->body);
    my $output = `identify -verbose $path | grep comment`;
    _corrupt "Flag not found" unless $output =~ /$flag/;
    $log->info('Found flag');

    return $SERVICE_OK;
}

sub put {
    my ( $id, $flag ) = @_;

    $log->info('Creating user ...');
    $url->path->merge('/create');
    $_ = _check_http_response( $ua->get($url) );
    my ($user_cookie) = grep { $_->name eq 'user' } @{ $ua->cookie_jar->all };
    my ($sign_cookie) = grep { $_->name eq 'sign' } @{ $ua->cookie_jar->all };
    my $user = $user_cookie->value;
    my $sign = $sign_cookie->value;
    $log->info("Created user: '$user'");
    _mumble "Invalid main page" unless $_->text =~ /Upload new photo/;
    _mumble "Invalid main page" unless $_->text =~ /Latest photos/;

    $log->info('Generating image ...');
    my ( undef, $img ) = _image;

    $url->path->merge('/upload');
    $log->info('Uploading image ...');
    my $res = $ua->post(
        $url => form => {
            info  => $flag,
            image => {
                content        => $img,
                filename       => "$id.png",
                'Content-Type' => 'image/png'
            }
        }
    );
    $_ = _check_http_response($res);
    $log->info('Upload image');

    sleep 2;
    say "$user:$sign:$id";
    return $SERVICE_OK;
}
