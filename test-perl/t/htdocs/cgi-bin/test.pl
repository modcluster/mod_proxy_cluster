#!/usr/bin/perl
print "Content-Type: text/plain\n\n";

print "Fake App!\n";

foreach my $key (sort (keys %ENV)) {
    print "$key --> $ENV{$key}\n";
}
