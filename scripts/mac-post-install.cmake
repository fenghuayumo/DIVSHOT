cmake_minimum_required(VERSION 3.12)

# We want to explicitly symlink the newly installed app bundle
# such that the icon and executable match up correctly.
execute_process(COMMAND sudo rm -f /usr/local/bin/diverseshot)
execute_process(COMMAND sudo ln -s /Applications/diverseshot.app/Contents/Resources/mac-run-diverseshot.sh /usr/local/bin/diverseshot)
