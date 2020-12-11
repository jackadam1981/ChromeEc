// Copyright 2020 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// const MONITOR_CHILD_WINDOW_INTERVAL_MS = 500;
// const MONITOR_CHILD_WINDOW_TIMEOUT_MS = 60000;
const challengeURL =
    'https://chromeos.google.com/partner/console/cr50reset?challenge=';

const state = {};
const setStateAndRender = (newState) => {
  Object.assign(state, newState);
  try {
    render();
  } catch(error) {
    console.error(error);
    alert(error);
  }
};

const fetchAPI = (path, data={}) => {
  return fetch(path, {method: 'POST', headers: {'Content-Type': 'application/json'}, body: JSON.stringify(data)});
};

const renderBoolean =
     (id, value, trueText, trueColor, falseText, falseColor) => {
      const ele = document.getElementById(id);
      if (value === undefined) {
        ele.innerText = 'undefined';
        return;
      }
      ele.innerText = value ? trueText : falseText;
      ele.style.color = value ? trueColor : falseColor;
    };
const renderTable = (ele, data) => {
  const table = ele.appendChild(document.createElement('table'));
  for (let key in data) {
    const tr = table.appendChild(document.createElement('tr'));
    const tdKey = tr.appendChild(document.createElement('td'));
    const tdValue = tr.appendChild(document.createElement('td'));
    tdKey.innerText = key
    tdValue.innerText = data[key]
  }
};
const renderButton = (ele, text, onclick) => {
  const btn = ele.appendChild(document.createElement('button'));
  if (state.isLoading) {
    text += ' (Loading...)';
  }
  btn.innerText = text;
  btn.onclick = onclick;
  btn.disabled = state.isLoading;
};
const renderLink = (ele, text, href, target) => {
  const a = ele.appendChild(document.createElement('a'));
  a.href = href;
  a.innerText = text;
  a.target = target;
};
const renderInput = (ele, id, label) => {
  const div = ele.appendChild(document.createElement('div'));
  const b = div.appendChild(document.createElement('b'));
  b.innerText = label + ': ';
  const input = div.appendChild(document.createElement('input'));
  input.id = id;
  input.type = 'text';
  input.value = state['input-' + id] || '';
  input.onchange = e => {
    state['input-' + id] = e.target.value;
  };
};
const handleScan = async () => {
  setStateAndRender({isLoading: true});
  const resp = await fetchAPI('/scan');
  setStateAndRender({isLoading: false, scanData: await resp.json()});
};
const renderScan = (ele) => {
  renderButton(ele, 'Scan', handleScan);
  renderTable(ele, state.scanData);
};
const handleUnlock = async () => {
  setStateAndRender({isLoading: true});
  const authcode = document.getElementById('authcode').value;
  console.log('TTT');
  console.log(authcode);
  const resp = await fetchAPI('/unlock', {cr50SerialName: state.scanData.cr50SerialName, authcode});
  setStateAndRender({isLoading: false, unlockResult: await resp.json()});
};
const renderUnlock = (ele) => {
  renderLink(ele, 'Unlock Link', challengeURL + state.scanData.challenge, 'challenge');
  renderButton(ele, 'Unlock', handleUnlock);
  renderInput(ele, 'authcode', 'Authcode');
  renderTable(ele, state.unlockResult);
};
const renderLock = (ele) => {
  ;
};
const renderTestlab = (ele) => {
  ;
};
const render = () => {
  const root = document.getElementById('root');
  while (root.firstChild) {
    root.removeChild(root.lastChild);
  }
  renderScan(root);
  if (!state.scanData)
    return;
  if (state.scanData.isRestricted) {
    renderUnlock(root);
  } else {
    renderLock(root);
    renderTestlab(root);
  }
//   const fields = ['tty', 'cr50Serial', 'hwid', 'serialNumber'];
//   for (const key of fields) {
//     document.getElementById(key).innerText = data.device[key];
//   }
//   document.getElementById('error').innerText = data.error;
//   renderBoolean(
//       'isRestricted', data.device.isRestricted, 'Lock', 'red', 'Open', 'green');
//   renderBoolean(
//       'isLockAfterExtracting', data.isLockAfterExtracting, 'Relock', 'red',
//       'Remain Open', 'green');

//   if (data.isNewChallenge) {
//     const child = window.open(challengeURL + data.device.challenge, '_blank');
//     if (!child) return;
//     const interval = setInterval(() => {
//       if (!child.closed) return;
//       clearInterval(interval);
//       location.reload();
//     }, MONITOR_CHILD_WINDOW_INTERVAL_MS);
//     setTimeout(() => {
//       clearInterval(interval);
//     }, MONITOR_CHILD_WINDOW_TIMEOUT_MS);
//   }
};
window.onload = async () => {
  render();
};
