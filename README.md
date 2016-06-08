# shiftwallet

Shift QT5 Wallet

ShiftWallet is a free software wallet/front-end for Shift Gshift*.

## Usage

Gshift 2.6.0+ is required to be running for ShiftWallet to work.

ShiftWallet should auto-detect gshift's IPC file/name and work "out of the box" as long as gshift is running.

If ShiftWallet fails to detect the IPC file/name you can specify it in the settings panel.

Shiftwallet will show gshift's syncing progress and only process blocks after it's done.

## License

ShiftWallet is licensed under the GPLv3 license. See LICENSE for more info.

## Donations

#### Flattr
[![Flattr this git repo](http://api.flattr.com/button/flattr-badge-large.png)](https://flattr.com/submit/auto?user_id=Almindor&url=https://github.com/almindor/shiftwallet&title=ShiftWallet&language=&tags=github&category=software)

#### Bitcoin
`1NcJoao879C1pSKvFqnUKD6wKtFpCMppP6`

#### Litecoin
`LcTfGmqpXCiG7UikBDTa4ZiJMS5cRxSXHm`

#### Ether
`0xc64b50db57c0362e27a32b65bd29363f29fdfa59`

## Development

### Requirements

Gshift 2.6.0+

Qt5.2+ with qmake

### Building

qmake -config release && make

### Roadmap

- 1.1+ add eth support
- 1.0 add gshift account backup and restore
- 0.9 add transaction history support [done]
- 0.8 initial release [done]

### Caveats & bugs

Only supported client at the moment is Gshift. Eth and others should work if you go to settings and set the IPC path/name properly.
