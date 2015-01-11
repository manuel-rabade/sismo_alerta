<?php

/*
  Copyright 2015 Manuel Rodrigo Rábade García <manuel@rabade.net>

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.
  
  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
  
  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

date_default_timezone_set('Mexico/General');

require_once(realpath(__DIR__) . '/config.php');
require_once(realpath(__DIR__) . '/vendor/twitter.class.php');

// parametros request
if (!isset($_GET['secret']) || $_GET['secret'] != $secret) {
  exit;
}
if (!isset($_GET['same'])) {
  exit;
}

// mensaje same
$bytes = explode(':', $_GET['same']);
$txt = '';
foreach ($bytes as $byte) {
  if (!empty($byte)) {
    if ($byte > 31 && $byte < 127) {
      $txt .= chr($byte);
    } else {
      $txt .= '.';
    }
  }
}
error_log("SAME = $txt");

// fecha y hora
$time = date('H:i:s');
$dir = date('Y/m/d');
$file = date('His') . '.txt';

// rutas y url
$path_dir = implode('/', array($log, $dir));
$path_file = implode('/', array($path_dir, $file));
$url = implode('/', array($base, $dir, $file));

// guardamos mensaje same
if (!is_dir($path_dir)) {
  mkdir($path_dir, 0755, true);
}
file_put_contents($path_file, $txt . "\n");

// examinamos mensaje same
if (preg_match('/-([A-Z]{3})-([A-Z]{3})-([0-9]{6})\+([0-9]{4})-([0-9]{7})-(.{8})-/i', $txt, $match)) {
  // construimos mensaje twitter
  $msg = '';
  if ($match[2] == 'RWT') {
    $msg = 'Prueba periódica @ ' . $time;
  } else if ($match[2] == 'EQW') {
    $msg = 'ALERTA SÍSMICA @ ' . $time;
  } else {
    $msg = 'Mensaje desconocido @ ' . $time;
  }
  $msg .= ', mensaje original: ' . $url;
  // publicamos mensaje en twitter
  $twitter = new Twitter($consumer_key, $consumer_secret, $access_token, $access_token_secret);
  $tweet = $twitter->send($msg);
}

?>
