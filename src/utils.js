export function buf2hex(buffer) { // buffer is an ArrayBuffer
  return Array.prototype.map.call(new Uint8Array(buffer), x => ('00' + x.toString(16)).slice(-2)).join('');
}

export function decodeUnicode(str) {
  str = str.replace(/\\/g, "%");
  return unescape(str);
}

export function hasChildren(box) {
  return [
    'trak',
    'mdia',
    'minf',
    'dinf',
    'stbl',
  ].includes(box.type)
}

export function extractChildren(parent) {
  let parentData = parent.data.slice(8)

  const children = []
  
  while (parentData.length) {
    const boxSize =  Number('0x' + Array.from(parentData.slice(0, 4)).map(n => ('00' + n.toString(16)).slice(-2)).join(''))
    const boxType = Array.from(parentData.slice(4, 8)).map(n => ('\\u00' + ('00' + n.toString(16)).slice(-2))).join('')
    const boxData = parentData.slice(0, boxSize)
    const box = {
      type: decodeUnicode(boxType),
      size: boxSize,
      data: boxData,
    }

    if (hasChildren(box)) {
      box.children = extractChildren(box)
    }

    children.push(box)

    parentData = parentData.slice(boxSize)
  }

  return children
}

export function toDigitFromUint8Array(uint8arr) {
  return Number('0x' + Array.from(uint8arr).map(n => ('00' + n.toString(16)).slice(-2)).join(''))
}

export function toBitsFromUint8Array(uint8arr) {
  return Array.from(uint8arr).map(n => ('00000000' + n.toString(2)).slice(-8)).join('')
}

export function getSamplePosition(stream) {
  const sampleCount = stream.stszCount
  const chunkCount = stream.stcoCount

  if (sampleCount > 0) {
    stream.samplePosition = []
  }

  let remainChunkCount = chunkCount
  let sampleIndex = 0

  for (let i = 0; i < stream.stscCount; i++) {
    let cCount = 0
    if (i !== stream.stscCount - 1) {
      cCount = stream.stscData[i + 1].firstChunk - stream.stscData[i].firstChunk
      remainChunkCount -= cCount
    } else {
      cCOunt = remainChunkCount
    }

    for (let j = 0; j < cCount; j++) {
      let chunkIndex = stream.stscData[i].firstChunk + j;
      let offset = stream.stcoData[chunkIndex - 1];
      for (let k = 0; k < stream.stscData[i].samplesPerChunk; k++) {
        stream.samplePosition[sampleIndex] = offset;
        offset += stream.stszData[sampleIndex];
        smapleIndex++;
        if (sampleIndex > sampleCount) {
          return;
        }
      }
    }
  }
}
