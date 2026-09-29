package com.libcna.house;

import org.libsdl.app.SDLActivity;

public final class HouseActivity extends SDLActivity {
    // The command line, as the Web page's ?arg= gives it: the launching intent's "args" string,
    // split at spaces, e.g. am start -n com.libcna.house/.HouseActivity --es args "--time=22 --no-audio"
    @Override
    protected String[] getArguments() {
        String line = getIntent().getStringExtra("args");
        return line == null || line.trim().isEmpty() ? new String[0] : line.trim().split("\\s+");
    }
}
