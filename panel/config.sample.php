<?php
// Copy this file to config.php and fill in your values.
return [
    // MySQL (create an empty database + user in cPanel / DirectAdmin first)
    'db_host' => 'localhost',
    'db_name' => 'petbot',
    'db_user' => 'petbot_user',
    'db_pass' => 'CHANGE_ME',

    // Optional: pull outside data (weather, prices, Hafez) through this server.
    'enable_proxy' => true,

    // Time zone shown in the panel
    'timezone' => 'Asia/Tehran',
];
