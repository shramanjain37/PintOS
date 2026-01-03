sub check_procon_rst {
    my ($num_of_string) = @_;
    our ($test);

    @output = read_text_file ("$test.output");
    common_checks ("run", @output);

    my ($output_line);

    foreach $line (@output) {
        #print "$line\n";
        if ($last_line =~ m/\(procon-(.*)\) begin/) {
            $output_line = $line;
            last;
        };
        $last_line = $line;
    }

    if ($num_of_string == 0) {
        if ($output_line eq "") {
            pass;
        } else {
            fail "Output should be empty or zero producer or zero consumer.";
        }
    }

    my %expected_char_count = (
        "H" => $num_of_string,
        "w" => $num_of_string,
        "d" => $num_of_string,
        "e" => $num_of_string,
        "r" => $num_of_string,
        " " => $num_of_string,
        "o" => 2*$num_of_string,
        "l" => 3*$num_of_string
    );

    my %char_count;

    #Split output and check length
    @char_array = split //, $output_line;
    $length = @char_array;
    fail "Incorrect output $output_line length $length, expect $num_of_string*11" if ($length != $num_of_string * 11);

    #Count chars' appearance
    foreach my $char (@char_array) {
        $char_count{$char}++;
    }

    #Check if chars appear the right times.
    foreach $correct_char (keys %char_count) {
        if (%char_count{$correct_char} != $expected_char_count{$correct_char}) {
            fail "Char $correct_char appears $char_count{$correct_char} time(s) incorrectly.\n";
        }
    }

    pass;

}

1;
