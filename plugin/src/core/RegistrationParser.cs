// Parsing EVT-REGISTRATION off the wire.
//
// This is the plugin's only untrusted input. Anything on the LAN can send a datagram to the fixed
// port, so every field is validated rather than trusted, and a malformed payload is dropped
// silently rather than logged — an attacker who can make the log grow has a denial of service, and
// a log full of junk is worse than no log.
//
// Hand-rolled rather than a JSON dependency, for the same reason as Frame.ToJson: the plugin must
// build with nothing but the Framework compiler, and a parser dependency would have to be shipped
// alongside. The trade is real and worth stating — a hand-rolled parser is a liability on
// untrusted input — so this one is deliberately minimal: it reads a FLAT object of string and
// number values, refuses nesting outright, and is bounded in both input size and field count
// before it begins.

using System;
using System.Collections.Generic;
using System.Globalization;
using System.Text;

namespace CydSimDash.Core
{
    public sealed class Registration
    {
        public int ProtocolMajor;
        public int ProtocolMinor;
        public string DeviceId;
        public string FirmwareVersion;
    }

    public static class RegistrationParser
    {
        /// <summary>A registration is a few dozen bytes. Anything larger is not one, and refusing
        /// early means the parser never runs on a payload designed to make it work hard.</summary>
        public const int MaxBytes = 512;

        private const int MaxFields = 16;
        private const int MaxStringLength = 64;

        public static bool TryParse(byte[] data, int length, out Registration result)
        {
            result = null;
            if (data == null || length <= 0 || length > MaxBytes) return false;

            string text;
            try { text = Encoding.UTF8.GetString(data, 0, length); }
            catch (Exception) { return false; }

            Dictionary<string, string> fields;
            if (!TryReadFlatObject(text, out fields)) return false;

            Registration r = new Registration();

            string v;
            if (!fields.TryGetValue("protocolMajor", out v) || !TryInt(v, out r.ProtocolMajor)) return false;
            if (!fields.TryGetValue("protocolMinor", out v) || !TryInt(v, out r.ProtocolMinor)) return false;
            if (!fields.TryGetValue("deviceId", out r.DeviceId)) return false;
            if (!DeviceTable.IsValidDeviceId(r.DeviceId)) return false;

            // Firmware version is informational, so its absence is tolerated. Its CONTENT is not
            // trusted: it is bounded here because it is carried into the device table.
            if (!fields.TryGetValue("firmwareVersion", out r.FirmwareVersion)) r.FirmwareVersion = null;
            if (r.FirmwareVersion != null && r.FirmwareVersion.Length > MaxStringLength) return false;

            result = r;
            return true;
        }

        private static bool TryInt(string s, out int value)
        {
            return int.TryParse(s, NumberStyles.Integer, CultureInfo.InvariantCulture, out value);
        }

        /// <summary>Read a flat JSON object into string values. Nesting is refused rather than
        /// skipped, because a parser that quietly tolerates structure it does not understand is
        /// how a parser becomes an attack surface.</summary>
        private static bool TryReadFlatObject(string s, out Dictionary<string, string> fields)
        {
            fields = new Dictionary<string, string>(StringComparer.Ordinal);
            int i = 0;
            SkipWhitespace(s, ref i);
            if (i >= s.Length || s[i] != '{') return false;
            i++;

            SkipWhitespace(s, ref i);
            if (i < s.Length && s[i] == '}') return true;      // an empty object parses, then fails on fields

            while (i < s.Length)
            {
                if (fields.Count >= MaxFields) return false;

                SkipWhitespace(s, ref i);
                string key;
                if (!TryReadString(s, ref i, out key)) return false;

                SkipWhitespace(s, ref i);
                if (i >= s.Length || s[i] != ':') return false;
                i++;
                SkipWhitespace(s, ref i);
                if (i >= s.Length) return false;

                string value;
                char c = s[i];
                if (c == '"')
                {
                    if (!TryReadString(s, ref i, out value)) return false;
                }
                else if (c == '{' || c == '[')
                {
                    return false;                              // nesting: refused outright
                }
                else
                {
                    if (!TryReadBareValue(s, ref i, out value)) return false;
                }

                fields[key] = value;

                SkipWhitespace(s, ref i);
                if (i >= s.Length) return false;
                if (s[i] == ',') { i++; continue; }
                if (s[i] == '}') return true;
                return false;
            }
            return false;
        }

        private static void SkipWhitespace(string s, ref int i)
        {
            while (i < s.Length && (s[i] == ' ' || s[i] == '\t' || s[i] == '\r' || s[i] == '\n')) i++;
        }

        private static bool TryReadString(string s, ref int i, out string value)
        {
            value = null;
            if (i >= s.Length || s[i] != '"') return false;
            i++;
            StringBuilder sb = new StringBuilder();
            while (i < s.Length)
            {
                char c = s[i++];
                if (c == '"') { value = sb.ToString(); return true; }
                if (c == '\\')
                {
                    if (i >= s.Length) return false;
                    char e = s[i++];
                    switch (e)
                    {
                        case '"': sb.Append('"'); break;
                        case '\\': sb.Append('\\'); break;
                        case '/': sb.Append('/'); break;
                        case 'n': sb.Append('\n'); break;
                        case 'r': sb.Append('\r'); break;
                        case 't': sb.Append('\t'); break;
                        case 'b': sb.Append('\b'); break;
                        case 'f': sb.Append('\f'); break;
                        case 'u':
                            if (i + 4 > s.Length) return false;
                            int cp;
                            if (!int.TryParse(s.Substring(i, 4), NumberStyles.HexNumber,
                                              CultureInfo.InvariantCulture, out cp)) return false;
                            sb.Append((char)cp);
                            i += 4;
                            break;
                        default: return false;
                    }
                }
                else
                {
                    sb.Append(c);
                }
                if (sb.Length > MaxStringLength) return false;
            }
            return false;                                       // unterminated
        }

        private static bool TryReadBareValue(string s, ref int i, out string value)
        {
            int start = i;
            while (i < s.Length && s[i] != ',' && s[i] != '}' &&
                   s[i] != ' ' && s[i] != '\t' && s[i] != '\r' && s[i] != '\n') i++;
            value = s.Substring(start, i - start);
            return value.Length > 0 && value.Length <= MaxStringLength;
        }
    }
}
