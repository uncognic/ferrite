/*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Ring {
    Supervisor = 0,
    User = 1,
}

impl Ring {
    pub fn from_u32(val: u32) -> Self {
        match val & 0b11 {
            0 => Ring::Supervisor,
            _ => Ring::User,
        }
    }

    pub fn is_supervisor(self) -> bool {
        self == Ring::Supervisor
    }
}
